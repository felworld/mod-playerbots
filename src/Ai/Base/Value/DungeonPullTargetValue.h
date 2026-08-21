/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DUNGEONPULLTARGETVALUE_H
#define PLAYERBOTS_DUNGEONPULLTARGETVALUE_H

#include "TargetValue.h"

class PlayerbotAI;
class Unit;

// The nearest unengaged hostile pack member in the tank's line of sight (Felworld).
class DungeonPullTargetValue : public TargetValue
{
public:
    DungeonPullTargetValue(PlayerbotAI* botAI, std::string const name = "dungeon pull target")
        : TargetValue(botAI, name)
    {
    }

    Unit* Calculate() override;
};

#endif
