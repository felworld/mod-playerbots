/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PARTYMEMBERTOBOOST_H
#define PLAYERBOTS_PARTYMEMBERTOBOOST_H

#include "PartyMemberValue.h"

class PlayerbotAI;
class Unit;

// The group member worth handing a damage cooldown to (Power Infusion and its kind): a caster
// dps that is in combat, spending mana, free to cast, and not already running a haste cooldown
// of its own. "party member without aura" would answer with whoever happens to be missing the
// buff - a warrior or a rogue as readily as a mage - which is how the buff gets wasted.
class PartyMemberToBoost : public PartyMemberValue
{
public:
    PartyMemberToBoost(PlayerbotAI* botAI, std::string const name = "party member to boost")
        : PartyMemberValue(botAI, name, 1000)
    {
    }

protected:
    Unit* Calculate() override;
    // Power Infusion reaches 30 yards; the shared party check allows twice the spell distance,
    // which is more than any of these cooldowns can reach.
    bool Check(Unit* unit) override;
};

#endif
