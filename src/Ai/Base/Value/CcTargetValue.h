/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CCTARGETVALUE_H
#define PLAYERBOTS_CCTARGETVALUE_H

#include "NamedObjectContext.h"
#include "ObjectGuid.h"
#include "TargetValue.h"

#include <unordered_map>

class PlayerbotAI;
class Unit;

class CcTargetValue : public TargetValue, public Qualified
{
public:
    CcTargetValue(PlayerbotAI* botAI, std::string const name = "cc target") : TargetValue(botAI, name) {}

    Unit* Calculate() override;

private:
    Unit* FindBreatherTarget();
};

// What one bot has spent on crowd controlling one mob during the current fight (Felworld).
struct CcRecastInfo
{
    uint32 casts = 0;       // controls landed on it since the ledger was last cleared
    uint32 lastCastMs = 0;  // last application, for pruning
};

// Per-bot ledger of the mobs this bot has crowd controlled in the fight it is in. Keyed by GUID on
// purpose: a mob can die, despawn or leave the map between ticks, so nothing kept here may be a
// pointer. Dropping out of combat wipes it, so the next pull starts on a fresh budget.
class CcRecastMemory
{
public:
    void Refresh(bool inCombat, uint32 now);

    uint32 Casts(ObjectGuid guid) const;
    void Note(ObjectGuid guid, uint32 now);

private:
    std::unordered_map<ObjectGuid, CcRecastInfo> entries;
    uint32 lastPrune = 0;
    bool wasInCombat = false;
};

class CcRecastMemoryValue : public ManualSetValue<CcRecastMemory&>
{
public:
    CcRecastMemoryValue(PlayerbotAI* botAI, std::string const name = "cc recast memory")
        : ManualSetValue<CcRecastMemory&>(botAI, memory, name)
    {
    }

private:
    CcRecastMemory memory;
};

// The mob is still worth taking out of the fight (Felworld): the group has not committed to killing
// it - it is near full health and nobody is swinging at it - and this bot has recast budget left on
// it. Enemy players answer on the budget alone; diminishing returns already price their re-controls,
// and the solo breather case wants an opponent that is losing.
bool IsWorthCrowdControlling(PlayerbotAI* botAI, Unit* target);

// Spends one slot of the recast budget. Called when a control is actually cast, not when one is
// merely considered.
void NoteCrowdControlCast(PlayerbotAI* botAI, Unit* target);

#endif
