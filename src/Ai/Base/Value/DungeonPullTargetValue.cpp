/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DungeonPullTargetValue.h"
#include "Playerbots.h"
#include <cmath>

Unit* DungeonPullTargetValue::Calculate()
{
    if (!IsInstancedGroupContent(bot))
        return nullptr;

    // "possible targets" is already vetted (attackable, visible, in line of sight, within sight distance).
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");

    Unit* result = nullptr;
    float bestDistance = 0.0f;
    for (ObjectGuid const guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsInWorld() || unit->IsDuringRemoveFromWorld() || !unit->IsAlive())
            continue;

        Creature* creature = unit->ToCreature();
        if (!creature || creature->GetCreatureType() == CREATURE_TYPE_CRITTER)
            continue;

        // Neutral NPCs (vendors, quest givers, escorts) are not a pull.
        if (!bot->IsHostileTo(unit))
            continue;

        // Somebody else's fight already.
        if (unit->IsInCombat())
            continue;

        // Another floor of the instance, not the next pack.
        if (std::fabs(bot->GetPositionZ() - unit->GetPositionZ()) > 10.0f)
            continue;

        float const distance = bot->GetDistance(unit);
        if (!result || distance < bestDistance)
        {
            bestDistance = distance;
            result = unit;
        }
    }

    return result;
}
