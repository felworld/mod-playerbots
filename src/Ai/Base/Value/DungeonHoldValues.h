/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DUNGEONHOLDVALUES_H
#define PLAYERBOTS_DUNGEONHOLDVALUES_H

#include "ObjectGuid.h"
#include "Value.h"

#include <unordered_map>

class PlayerbotAI;
class Unit;

// What one bot has watched one mob do since the group's fight with it started (Felworld).
struct TankEngageInfo
{
    uint32 firstInTankMelee = 0;  // first tick it was seen inside the main tank's melee range, 0 = not there now
    uint32 combatStart = 0;       // first tick it was seen in combat at all
    uint32 lastSeen = 0;          // last evaluation, for pruning
};

// Per-bot memory of the mobs it is holding fire on. Keyed by GUID on purpose: a mob can die, despawn
// or leave the map between ticks, so nothing kept here may be a pointer.
class TankEngageMemory
{
public:
    void Prune(uint32 now);

    std::unordered_map<ObjectGuid, TankEngageInfo> entries;

private:
    uint32 lastPrune = 0;
};

class TankEngageMemoryValue : public ManualSetValue<TankEngageMemory&>
{
public:
    TankEngageMemoryValue(PlayerbotAI* botAI, std::string const name = "tank engage memory")
        : ManualSetValue<TankEngageMemory&>(botAI, memory, name)
    {
    }

private:
    TankEngageMemory memory;
};

// The hold applies to this bot at all: enabled, instanced group content, and a live main tank other
// than the bot itself to wait for (Felworld).
bool IsDungeonHoldActive(PlayerbotAI* botAI);

// This bot must withhold offense against target - the main tank has not picked it up yet.
bool ShouldHoldForTank(PlayerbotAI* botAI, Unit* target);

#endif
