/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HealthTriggers.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"

namespace
{
// An off-role heal is only ever an emergency stopgap: below this the target is a global or two from
// dead, and a wasted rotation is cheaper than a corpse.
constexpr float OFF_ROLE_HEAL_EMERGENCY_PCT = 35.0f;

// A healer this far into its mana bar cannot be counted on to cover the target, so the off-role bot
// steps in anyway.
constexpr float OFF_ROLE_HEAL_HEALER_MIN_MANA_PCT = 15.0f;

// Somebody whose job this is can still take the heal: a living healer-spec group member, on our map,
// within heal range of the target, with mana left to cast. Spec, not assigned strategy - a DPS bot
// carrying an offheal overlay is exactly who this gate is for.
bool HealerCoversTarget(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    float const healRange = botAI->GetRange("heal");

    Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (!member || member == bot || !member->IsAlive() || member->GetMap() != bot->GetMap())
            continue;

        if (!PlayerbotAI::IsHeal(member, true))
            continue;

        if (member->getPowerType() == POWER_MANA &&
            member->GetPowerPct(POWER_MANA) < OFF_ROLE_HEAL_HEALER_MIN_MANA_PCT)
            continue;

        if (member->GetDistance(target) > healRange)
            continue;

        return true;
    }

    return false;
}
}  // namespace

bool OffRoleHealBlocked(PlayerbotAI* botAI, Unit* target)
{
    if (!botAI || !target)
        return false;

    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    if (PlayerbotAI::IsHeal(bot, true))
        return false;

    // "party member to heal" resolves to the bot itself when it is the worst off (and always when
    // solo). Patching its own health is survival, not main-healing.
    if (target == bot)
        return false;

    if (!bot->IsInCombat())
        return false;

    if (target->GetHealthPct() >= OFF_ROLE_HEAL_EMERGENCY_PCT)
        return true;

    return HealerCoversTarget(botAI, bot, target);
}

bool PartyMemberLowHealthTrigger::IsActive()
{
    if (!HealthInRangeTrigger::IsActive())
        return false;

    return !OffRoleHealBlocked(botAI, GetTarget());
}

bool HealthInRangeTrigger::IsActive()
{
    // Spirit of Redemption leaves a priest alive at 1 hp until the aura expires and kills them. Their
    // health is not restorable, so treat them as needing nothing -- otherwise a priest in that form
    // sees itself at ~0% and fires every self-heal/shield/healthstone trigger it has.
    Unit* target = GetTarget();
    if (target && target->HasSpiritOfRedemptionAura())
        return false;

    return ValueInRangeTrigger::IsActive() && !AI_VALUE2(bool, "dead", GetTargetName());
}

float HealthInRangeTrigger::GetValue() { return AI_VALUE2(uint8, "health", GetTargetName()); }

bool PartyMemberDeadTrigger::IsActive() { return GetTarget(); }

bool CombatPartyMemberDeadTrigger::IsActive() { return GetTarget(); }

bool DeadTrigger::IsActive() { return AI_VALUE2(bool, "dead", GetTargetName()); }

bool AoeHealTrigger::IsActive() { return AI_VALUE2(uint8, "aoe heal", type) >= count; }

bool HealerLowManaTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    return target->GetPowerPct(POWER_MANA) < sPlayerbotAIConfig.lowMana;
}

bool AoeInGroupTrigger::IsActive()
{
    // An off-role bot has no business dropping a group heal mid-fight either. Unlike a single-target
    // heal there is no one dying party member to make an emergency exception for, so the rotation
    // simply wins (Felworld).
    if (bot->IsInCombat() && !PlayerbotAI::IsHeal(bot, true))
        return false;

    int32 member = botAI->GetNearGroupMemberCount();
    if (member < 5)
        return false;
    int threshold = member * 0.5;
    if (member <= 5)
        threshold = 3;
    else if (member <= 10)
        threshold = std::min(threshold, 5);
    else if (member <= 25)
        threshold = std::min(threshold, 10);
    else
        threshold = std::min(threshold, 15);

    return AI_VALUE2(uint8, "aoe heal", type) >= threshold;
}
