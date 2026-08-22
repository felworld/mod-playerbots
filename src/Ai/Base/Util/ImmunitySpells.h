/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_IMMUNITYSPELLS_H
#define PLAYERBOTS_IMMUNITYSPELLS_H

#include "Define.h"

#include <string>
#include <vector>

// The immunities "cancel immunity" knows how to drop (see ImmunityTriggers.h). Spell ids rather
// than names, since Divine Intervention and Hand of Protection land on bots of every class and the
// name lookup only covers spells the bot itself knows (Felworld).
namespace ai::immunity
{
    constexpr uint32 SPELL_ICE_BLOCK = 45438;
    constexpr uint32 SPELL_DIVINE_SHIELD = 642;
    constexpr uint32 SPELL_DISPERSION = 47585;
    constexpr uint32 SPELL_DIVINE_INTERVENTION = 19752;

    // All three ranks of Hand of Protection (the 3.3.5 name of Blessing of Protection).
    inline std::vector<uint32> HandOfProtectionRanks() { return { 1022, 5599, 10278 }; }

    // The triggers whose immunity casts are survival moves the bot may cut short once it is safe.
    // An immunity fired by anything else - a raid strategy dodging a mechanic, the master's "cast"
    // command - is left to run its course, since only its author knows when it has done its job.
    inline bool IsSurvivalTrigger(std::string const& trigger)
    {
        return trigger == "critical health" || trigger == "low health" || trigger == "low mana";
    }
}

#endif
