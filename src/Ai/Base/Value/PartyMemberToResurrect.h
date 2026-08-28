/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PARTYMEMBERTORESURRECT_H
#define PLAYERBOTS_PARTYMEMBERTORESURRECT_H

#include "PartyMemberValue.h"

class PlayerbotAI;
class Unit;

class PartyMemberToResurrect : public PartyMemberValue
{
public:
    PartyMemberToResurrect(PlayerbotAI* botAI, std::string const name = "party member to resurrect")
        : PartyMemberValue(botAI, name)
    {
    }

protected:
    Unit* Calculate() override;
};

// Corpse worth a jumper-cables attempt: empty while any living group member
// knows a real resurrection spell, and prefers a dead resurrecter otherwise,
// so one successful jump-start can recover the whole group.
class PartyMemberToJumperCable : public PartyMemberValue
{
public:
    PartyMemberToJumperCable(PlayerbotAI* botAI, std::string const name = "party member to jumper cable")
        : PartyMemberValue(botAI, name)
    {
    }

protected:
    Unit* Calculate() override;
};

#endif
