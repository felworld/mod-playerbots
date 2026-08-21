/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ImmunityTriggers.h"

#include "PlayerbotAIConfig.h"
#include "Playerbots.h"

namespace
{
    // Longer than any of the stamped immunities lasts (Divine Shield: 12 s), shorter than their
    // cooldowns, so a stamp can only ever belong to the aura currently up.
    constexpr time_t SURVIVAL_CAST_WINDOW = 15;
}

bool OutlivedImmunityTrigger::IsActive()
{
    for (uint32 spellId : spellIds)
        if (bot->HasAura(spellId))
            return Outlived();

    return false;
}

bool OutlivedImmunityTrigger::OutOfCombat() { return !AI_VALUE2(bool, "combat", "self target"); }

bool OutlivedImmunityTrigger::SurvivalCast()
{
    time_t const stamp = AI_VALUE(time_t, "emergency immunity time");
    return stamp && time(nullptr) - stamp <= SURVIVAL_CAST_WINDOW;
}

uint8 OutlivedImmunityTrigger::Health() { return AI_VALUE2(uint8, "health", "self target"); }

uint8 OutlivedImmunityTrigger::Mana() { return AI_VALUE2(uint8, "mana", "self target"); }

IceBlockOutlivedTrigger::IceBlockOutlivedTrigger(PlayerbotAI* botAI)
    : OutlivedImmunityTrigger(botAI, "ice block outlived", { ai::immunity::SPELL_ICE_BLOCK }) {}

// Blocked at critical health, healed out of the low band: ten seconds of standing still is worth
// nothing more. Out of combat the block only pins the bot in place.
bool IceBlockOutlivedTrigger::Outlived()
{
    return OutOfCombat() || (SurvivalCast() && Health() >= sPlayerbotAIConfig.mediumHealth);
}

DivineShieldOutlivedTrigger::DivineShieldOutlivedTrigger(PlayerbotAI* botAI)
    : OutlivedImmunityTrigger(botAI, "divine shield outlived", { ai::immunity::SPELL_DIVINE_SHIELD }) {}

// The paladin keeps acting under the bubble, and "divine shield low health" heals it up to 80%
// underneath, so the bar is higher than Ice Block's: once nearly full, the halved damage and the
// mobs ignoring the (tank) paladin cost more than the remaining seconds of immunity are worth.
bool DivineShieldOutlivedTrigger::Outlived()
{
    return OutOfCombat() || (SurvivalCast() && Health() >= sPlayerbotAIConfig.almostFullHealth);
}

DispersionOutlivedTrigger::DispersionOutlivedTrigger(PlayerbotAI* botAI)
    : OutlivedImmunityTrigger(botAI, "dispersion outlived", { ai::immunity::SPELL_DISPERSION }) {}

// Dispersion is both the shadow priest's panic button and its mana battery; it has done its job
// once neither is pressing.
bool DispersionOutlivedTrigger::Outlived()
{
    return OutOfCombat() || (SurvivalCast() && Health() >= sPlayerbotAIConfig.mediumHealth &&
                             Mana() >= sPlayerbotAIConfig.mediumMana);
}

DivineInterventionOutlivedTrigger::DivineInterventionOutlivedTrigger(PlayerbotAI* botAI)
    : OutlivedImmunityTrigger(botAI, "divine intervention outlived",
                              { ai::immunity::SPELL_DIVINE_INTERVENTION }) {}

// Cast on the bot by a paladin to survive a wipe. It exists to be cancelled once the mobs have
// reset, so the survivor can resurrect the group - three minutes of waiting otherwise.
bool DivineInterventionOutlivedTrigger::Outlived() { return OutOfCombat(); }

HandOfProtectionOutlivedTrigger::HandOfProtectionOutlivedTrigger(PlayerbotAI* botAI)
    : OutlivedImmunityTrigger(botAI, "hand of protection outlived", ai::immunity::HandOfProtectionRanks()) {}

// Physical immunity that also stops the wearer from melee or ranged attacks, and makes mobs drop
// it as a target. Casters lose nothing and keep it; a tank or physical damage dealer drops it once
// healed back up, or once there is nothing left to be protected from.
bool HandOfProtectionOutlivedTrigger::Outlived()
{
    if (!botAI->IsTank(bot) && !botAI->IsMelee(bot) && bot->getClass() != CLASS_HUNTER)
        return false;

    return OutOfCombat() || Health() >= sPlayerbotAIConfig.almostFullHealth;
}
