/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ImmunityActions.h"

#include "ImmunitySpells.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"

bool CastEmergencyImmunityAction::Execute(Event event)
{
    bool const survivalCast = AI_VALUE2(uint8, "health", "self target") < sPlayerbotAIConfig.lowHealth;

    if (!CastBuffSpellAction::Execute(event))
        return false;

    if (survivalCast)
        context->GetValue<time_t>("emergency immunity time")->Set(time(nullptr));

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
