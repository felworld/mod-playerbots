/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RogueOpeningActions.h"
#include "AttackersValue.h"
#include "Playerbots.h"

namespace
{
// Sap reaches a little past melee; the bot closes the rest while sneaking in anyway.
constexpr float SAP_RANGE = 10.0f;
}

Value<Unit*>* CastSapAction::GetTargetValue() { return context->GetValue<Unit*>("cc target", getName()); }

Unit* CastSapOpenerAction::FindSapTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot->HasStealthAura() || bot->IsInCombat())
        return nullptr;

    // Only while the bot is already committed to a player fight - otherwise a stealthed rogue
    // would sap whoever happened to walk past.
    Unit* mark = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (!mark || !mark->IsPlayer() || !mark->IsAlive() || bot->IsFriendlyTo(mark))
        return nullptr;

    GuidVector enemies = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (ObjectGuid const& guid : enemies)
    {
        Unit* enemy = botAI->GetUnit(guid);
        if (!enemy || enemy == mark || !enemy->IsAlive() || enemy->IsInCombat())
            continue;

        if (AttackersValue::IsCrowdControlled(enemy))
            continue;

        if (bot->GetDistance(enemy) > SAP_RANGE)
            continue;

        if (!botAI->CanCastSpell("sap", enemy))
            continue;

        return enemy;
    }

    return nullptr;
}
