/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "CcTargetValue.h"
#include "Action.h"
#include "AiObjectContext.h"
#include "AttackersValue.h"
#include "Group.h"
#include "PlayerbotAI.h"
#include "ServerFacade.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
// A mob missing more than a sliver of its health is one the group has already committed to burning
// down - controlling it now only delays the kill, and the next tick of damage breaks the control
// again. Deliberately high: the point is "untouched", not "healthy".
constexpr uint8 CC_TARGET_MIN_HEALTH_PCT = 90;

// A control that gets broken can be re-applied a couple of times - a stray cleave or a dot tick is
// bad luck, not a verdict. Past that the mob has proved it will not stay controlled, and further
// recasts are the loop players notice.
constexpr uint32 CC_MAX_CASTS_PER_TARGET = 3;

// Chain pulls can keep a bot in combat indefinitely, so entries also age out on their own.
constexpr uint32 CC_RECAST_ENTRY_LIFETIME = 60 * IN_MILLISECONDS;
constexpr uint32 CC_RECAST_PRUNE_INTERVAL = 10 * IN_MILLISECONDS;

// Somebody in the group has already committed to killing this mob: a group member, or a pet of one,
// is swinging at it. That covers the tank's current victim, which is what a bot with an eye on the
// fight would read first. It deliberately does not ask who the mob is attacking - on a pull the
// whole pack runs at the tank, and those adds are exactly what the control is for.
bool IsGroupCommittedTo(Player* bot, Unit* target)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (Unit* attacker : target->getAttackers())
    {
        if (!attacker || !attacker->IsAlive() || attacker == bot)
            continue;

        Unit* owner = attacker->GetCharmerOrOwner();
        Player* member = owner ? owner->ToPlayer() : attacker->ToPlayer();
        if (member && member != bot && group->IsMember(member->GetGUID()))
            return true;
    }

    return false;
}

// A control effect the target would shrug off: third application in the diminishing-returns
// window (25% duration) or outright immune. Only players and pets diminish, so creatures always
// pass.
bool IsDiminished(PlayerbotAI* botAI, std::string const& spell, Unit* target)
{
    uint32 spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    DiminishingGroup group = GetDiminishingReturnsGroupForSpell(spellInfo, false);
    if (group == DIMINISHING_NONE)
        return false;

    return target->GetDiminishing(group) >= DIMINISHING_LEVEL_3;
}

// Guards shared by every candidate: castable, not already controlled, not diminished, healthy
// enough that the (regenerating) CC time isn't a gift, and clear of our own AoE.
bool PassesCommonGuards(PlayerbotAI* botAI, std::string const& spell, Unit* target)
{
    if (!botAI->CanCastSpell(spell, target))
        return false;

    if (AttackersValue::IsCrowdControlled(target) || IsDiminished(botAI, spell, target))
        return false;

    if (static_cast<uint8>(target->GetHealthPct()) < sPlayerbotAIConfig.mediumHealth)
        return false;

    AiObjectContext* context = botAI->GetAiObjectContext();
    if (*context->GetValue<uint8>("aoe count") > 2)
    {
        WorldLocation aoe = *context->GetValue<WorldLocation>("aoe position");
        if (ServerFacade::instance().IsDistanceLessOrEqualThan(
                ServerFacade::instance().GetDistance2d(target, aoe.GetPositionX(), aoe.GetPositionY()),
                sPlayerbotAIConfig.aoeRadius))
            return false;
    }

    return true;
}
}  // namespace

void CcRecastMemory::Refresh(bool inCombat, uint32 now)
{
    if (!inCombat)
    {
        // One wipe on the way out of combat, not one per tick: a control landed on an opener has to
        // survive until the fight it opened actually ends.
        if (wasInCombat)
            entries.clear();

        wasInCombat = false;
        return;
    }

    wasInCombat = true;

    if (lastPrune && getMSTimeDiff(lastPrune, now) < CC_RECAST_PRUNE_INTERVAL)
        return;

    lastPrune = now;

    for (auto itr = entries.begin(); itr != entries.end();)
    {
        if (getMSTimeDiff(itr->second.lastCastMs, now) >= CC_RECAST_ENTRY_LIFETIME)
            itr = entries.erase(itr);
        else
            ++itr;
    }
}

uint32 CcRecastMemory::Casts(ObjectGuid guid) const
{
    auto itr = entries.find(guid);
    return itr == entries.end() ? 0 : itr->second.casts;
}

void CcRecastMemory::Note(ObjectGuid guid, uint32 now)
{
    CcRecastInfo& info = entries[guid];
    ++info.casts;
    info.lastCastMs = now;
}

bool IsWorthCrowdControlling(PlayerbotAI* botAI, Unit* target)
{
    if (!botAI || !target || !target->IsAlive())
        return false;

    Player* bot = botAI->GetBot();
    uint32 const now = getMSTime();

    CcRecastMemory& memory = botAI->GetAiObjectContext()->GetValue<CcRecastMemory&>("cc recast memory")->Get();
    memory.Refresh(bot->IsInCombat(), now);

    if (memory.Casts(target->GetGUID()) >= CC_MAX_CASTS_PER_TARGET)
        return false;

    if (target->IsPlayer())
        return true;

    if (static_cast<uint8>(target->GetHealthPct()) < CC_TARGET_MIN_HEALTH_PCT)
        return false;

    return !IsGroupCommittedTo(bot, target);
}

void NoteCrowdControlCast(PlayerbotAI* botAI, Unit* target)
{
    if (!botAI || !target)
        return;

    CcRecastMemory& memory = botAI->GetAiObjectContext()->GetValue<CcRecastMemory&>("cc recast memory")->Get();
    memory.Note(target->GetGUID(), getMSTime());
}

class FindTargetForCcStrategy : public FindTargetStrategy
{
public:
    FindTargetForCcStrategy(PlayerbotAI* botAI, std::string const spell)
        : FindTargetStrategy(botAI), spell(spell), maxDistance(0.f), bestScore(-1)
    {
    }

public:
    void CheckAttacker(Unit* creature, ThreatManager* /*threatMgr*/) override
    {
        Player* bot = botAI->GetBot();
        AiObjectContext* context = botAI->GetAiObjectContext();

        if (*context->GetValue<Unit*>("rti cc target") == creature)
        {
            // A raid icon still does not license re-controlling a mob the group is already killing.
            if (IsWorthCrowdControlling(botAI, creature) && botAI->CanCastSpell(spell, creature))
                result = creature;
            return;
        }

        // The unit we are fighting is handled by CcTargetValue::FindBreatherTarget (1v1 only)
        if (*context->GetValue<Unit*>("current target") == creature)
            return;

        if (!PassesCommonGuards(botAI, spell, creature) || !IsWorthCrowdControlling(botAI, creature))
            return;

        Group* group = bot->GetGroup();
        if (!group)
        {
            // Solo with several attackers: take the most dangerous one out of the fight - a
            // player over a mob, a mana user (healer or caster) over a melee, the farther one
            // when tied so the one already on us keeps our attention.
            int32 score = (creature->IsControlledByPlayer() ? 2 : 0) +
                          (creature->getPowerType() == POWER_MANA ? 1 : 0);
            float distance = ServerFacade::instance().GetDistance2d(bot, creature);
            if (!result || score > bestScore || (score == bestScore && distance > maxDistance))
            {
                result = creature;
                bestScore = score;
                maxDistance = distance;
            }
            return;
        }

        // Grouped: keep the add that is farthest from our tanks, i.e. the one least likely to be
        // picked up
        float minDistance = botAI->GetRange("spell");

        uint32 tankCount = 0;
        uint32 dpsCount = 0;
        GetPlayerCount(creature, &tankCount, &dpsCount);
        if (!tankCount || !dpsCount)
        {
            result = creature;
            return;
        }

        Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
        for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
        {
            Player* member = ObjectAccessor::FindPlayer(itr->guid);
            if (!member || !member->IsAlive() || member == bot)
                continue;

            if (!botAI->IsTank(member))
                continue;

            float distance = ServerFacade::instance().GetDistance2d(member, creature);
            if (distance < minDistance)
                minDistance = distance;
        }

        if (!result || minDistance > maxDistance)
        {
            result = creature;
            maxDistance = minDistance;
        }
    }

private:
    std::string const spell;
    float maxDistance;
    int32 bestScore;
};

Unit* CcTargetValue::Calculate()
{
    FindTargetForCcStrategy strategy(botAI, qualifier);
    if (Unit* target = FindTarget(&strategy))
        return target;

    return FindBreatherTarget();
}

// Alone against a single player and losing: control them to reset the fight. The CC drops our
// target (InvalidTargetValue) and ends PvP combat a few seconds later, which is exactly the
// window the non-combat engine needs to heal, bandage or drink. Mirror-image situations where
// the opponent is hurt too are excluded by the mediumHealth guard - finishing them beats
// handing them a regenerating time-out. Class-specific setups on a healthy bot (Polymorph into
// Pyroblast, Fear into DoTs) are the class strategies' business, not this value's.
Unit* CcTargetValue::FindBreatherTarget()
{
    if (bot->GetGroup())
        return nullptr;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (!target || !target->IsControlledByPlayer())
        return nullptr;

    GuidVector attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    if (attackers.size() > 1)
        return nullptr;

    bool lowHealth = bot->GetHealthPct() < sPlayerbotAIConfig.mediumHealth;
    bool lowMana = bot->getPowerType() == POWER_MANA && bot->GetMaxPower(POWER_MANA) &&
                   bot->GetPower(POWER_MANA) * 100 / bot->GetMaxPower(POWER_MANA) < sPlayerbotAIConfig.lowMana;
    if (!lowHealth && !lowMana)
        return nullptr;

    if (!PassesCommonGuards(botAI, qualifier, target) || !IsWorthCrowdControlling(botAI, target))
        return nullptr;

    return target;
}
