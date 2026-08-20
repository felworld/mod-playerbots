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

namespace
{
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
            if (botAI->CanCastSpell(spell, creature))
                result = creature;
            return;
        }

        // The unit we are fighting is handled by CcTargetValue::FindBreatherTarget (1v1 only)
        if (*context->GetValue<Unit*>("current target") == creature)
            return;

        if (!PassesCommonGuards(botAI, spell, creature))
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

    if (!PassesCommonGuards(botAI, qualifier, target))
        return nullptr;

    return target;
}
