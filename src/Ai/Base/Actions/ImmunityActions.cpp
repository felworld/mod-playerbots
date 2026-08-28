/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ImmunityActions.h"

#include "GameTime.h"
#include "ImmunitySpells.h"
#include "ImmunityValues.h"
#include "Playerbots.h"

bool CastEmergencyImmunityAction::Execute(Event event)
{
    if (!CastBuffSpellAction::Execute(event))
        return false;

    // The event's source is the trigger that fired this action (the engine passes it through
    // prerequisite and alternative chains unchanged).
    context->GetValue<ImmunityCast>("immunity cast")->Set({ event.GetSource(), GameTime::GetGameTime().count() });
    return true;
}

bool CancelImmunityAction::isUseful()
{
    for (uint32 spellId : spellIds)
        if (bot->HasAura(spellId))
            return true;

    return false;
}

bool CancelImmunityAction::Execute(Event /*event*/)
{
    for (uint32 spellId : spellIds)
        if (bot->HasAura(spellId))
            bot->RemoveOwnedAura(spellId, ObjectGuid::Empty, 0, AURA_REMOVE_BY_CANCEL);

    return true;
}

CancelIceBlockAction::CancelIceBlockAction(PlayerbotAI* botAI)
    : CancelImmunityAction(botAI, "cancel ice block", { ai::immunity::SPELL_ICE_BLOCK }) {}

CancelDivineShieldAction::CancelDivineShieldAction(PlayerbotAI* botAI)
    : CancelImmunityAction(botAI, "cancel divine shield", { ai::immunity::SPELL_DIVINE_SHIELD }) {}

CancelDispersionAction::CancelDispersionAction(PlayerbotAI* botAI)
    : CancelImmunityAction(botAI, "cancel dispersion", { ai::immunity::SPELL_DISPERSION }) {}

CancelDivineInterventionAction::CancelDivineInterventionAction(PlayerbotAI* botAI)
    : CancelImmunityAction(botAI, "cancel divine intervention", { ai::immunity::SPELL_DIVINE_INTERVENTION }) {}

CancelHandOfProtectionAction::CancelHandOfProtectionAction(PlayerbotAI* botAI)
    : CancelImmunityAction(botAI, "cancel hand of protection", ai::immunity::HandOfProtectionRanks()) {}

bool CancelBanishAction::Execute(Event /*event*/)
{
    BanishedTarget const banished = FindBanishedTarget(botAI);
    if (!banished)
        return false;

    // The record is left standing: the aura only goes when the re-cast lands, so an interrupted
    // cast has to be retried, and a landed one already reads as "nothing banished".
    return botAI->CastSpell(banished.spellId, banished.unit);
}

bool ImmunityStandoffAction::isUseful()
{
    // Past the retreat step the gap is already open - stop walking and let the recovery
    // triggers have the window.
    Unit* enemy = AI_VALUE(Unit*, "immune enemy near");
    return enemy && bot->GetDistance(enemy) < ai::immunity::STANDOFF_RETREAT_STEP;
}

bool ImmunityStandoffAction::Execute(Event /*event*/)
{
    Unit* enemy = AI_VALUE(Unit*, "immune enemy near");
    if (!enemy)
        return false;

    return MoveAway(enemy, ai::immunity::STANDOFF_RETREAT_STEP, false);
}
