/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NearestUnitsValue.h"

#include "Playerbots.h"

GuidVector NearestUnitsValue::Calculate()
{
    std::list<Unit*> targets;
    FindUnits(targets);

    // The grid scan sees everything; a released ghost's client culls the alive
    // world (spirit healers, other ghosts and grouped teammates stay, plus the
    // core's units-near-corpse exception), so filter through core visibility while
    // ghosted. Kept ghost-gated rather than unconditional: raid modules read
    // server-side-invisible trigger creatures via these values as a stand-in for
    // the spell visuals a real client would render.
    bool ghost = bot->HasPlayerFlag(PLAYER_FLAGS_GHOST);

    GuidVector results;
    for (Unit* unit : targets)
    {
        if (ghost && !bot->CanSeeOrDetect(unit))
            continue;

        if (AcceptUnit(unit) && (ignoreLos || bot->IsWithinLOSInMap(unit)))
            results.push_back(unit->GetGUID());
    }

    return results;
}
