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

Unit* SnareTargetValue::Calculate()
{
    std::string const spell = qualifier;

    GuidVector attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        if (bot->GetDistance(unit) > botAI->GetRange("spell"))
            continue;

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
