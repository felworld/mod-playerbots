/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SnareTargetValue.h"
#include "AiObjectContext.h"
#include "PlayerbotAI.h"
#include "ServerFacade.h"
#include "TargetValue.h"

// A hostile player worth a snare: up and moving, not already rooted or slowed by anyone.
// Players have no chase/flee motion generators to read, so the mob heuristics below never
// see them; a human snares the player they're fighting as soon as it starts moving.
static bool IsPlayerSnareTarget(Player* bot, Unit* unit)
{
    return unit->IsPlayer() && unit->IsAlive() && !bot->IsFriendlyTo(unit) && unit->isMoving() &&
           !unit->HasUnitState(UNIT_STATE_STUNNED) && !unit->HasAuraType(SPELL_AURA_MOD_ROOT) &&
           !unit->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED);
}

Unit* SnareTargetValue::Calculate()
{
    std::string const spell = qualifier;
    float range = botAI->GetRange("spell");

    // The player the bot is actually fighting comes first
    Unit* current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (current && IsPlayerSnareTarget(bot, current) && bot->GetDistance(current) <= range)
        return current;

    GuidVector attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        if (bot->GetDistance(unit) > range)
            continue;

        if (unit->IsPlayer())
        {
            if (IsPlayerSnareTarget(bot, unit))
                return unit;
            continue;
        }

        // Covers fear as well as the two flee-for-assistance movements, so a low-health runner heading
        // for its friends gets snared just like a feared mob.
        if (IsFleeingFromCombat(unit))
            return unit;

        if (unit->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
        {
            Unit* chaseTarget = ServerFacade::instance().GetChaseTarget(unit);
            if (!chaseTarget)
                continue;
            Player* chaseTargetPlayer = ObjectAccessor::FindPlayer(chaseTarget->GetGUID());
            // check if need to snare
            bool shouldSnare = true;

            // do not slow down if bot is melee and mob/bot attack each other
            if (chaseTargetPlayer && !botAI->IsRanged(bot) && chaseTargetPlayer == bot)
                shouldSnare = false;

            if (!unit->isMoving())
                shouldSnare = false;

            if (unit->HasAuraType(SPELL_AURA_MOD_ROOT))
                shouldSnare = false;

            if (chaseTargetPlayer && shouldSnare && !botAI->IsTank(chaseTargetPlayer))
            {
                return unit;
            }
        }
    }

    return nullptr;
}
