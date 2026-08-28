/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_IMMUNITYACTIONS_H
#define PLAYERBOTS_IMMUNITYACTIONS_H

#include "GenericSpellActions.h"
#include "MovementActions.h"

#include <vector>

class PlayerbotAI;

// A self-immunity (Ice Block, Divine Shield, Dispersion). Records which trigger asked for the cast
// and when, so "cancel immunity" can tell a survival cast it may cut short from a raid strategy's
// mechanic dodge or the master's "cast" command, which it leaves alone (Felworld).
class CastEmergencyImmunityAction : public CastBuffSpellAction
{
public:
    CastEmergencyImmunityAction(PlayerbotAI* botAI, std::string const spell) : CastBuffSpellAction(botAI, spell) {}

    bool Execute(Event event) override;
};

// Cancels one of the listed auras on the bot the way a player right-clicks a buff off
// (AURA_REMOVE_BY_CANCEL). No cast, so it also works from inside Ice Block or Divine
// Intervention, where every spell is refused (Felworld).
class CancelImmunityAction : public Action
{
public:
    CancelImmunityAction(PlayerbotAI* botAI, std::string const name, std::vector<uint32> spellIds)
        : Action(botAI, name), spellIds(std::move(spellIds)) {}

    bool isUseful() override;
    bool Execute(Event event) override;

private:
    std::vector<uint32> spellIds;
};

class CancelIceBlockAction : public CancelImmunityAction
{
public:
    CancelIceBlockAction(PlayerbotAI* botAI);
};

class CancelDivineShieldAction : public CancelImmunityAction
{
public:
    CancelDivineShieldAction(PlayerbotAI* botAI);
};

class CancelDispersionAction : public CancelImmunityAction
{
public:
    CancelDispersionAction(PlayerbotAI* botAI);
};

class CancelDivineInterventionAction : public CancelImmunityAction
{
public:
    CancelDivineInterventionAction(PlayerbotAI* botAI);
};

class CancelHandOfProtectionAction : public CancelImmunityAction
{
public:
    CancelHandOfProtectionAction(PlayerbotAI* botAI);
};

// Releases the bot's own Banish by re-casting it on the mob that is under it: the mob is immune to
// the spell it is already wearing, and the core drops the aura on that immune hit
// (spell_warl_banish) - the same two clicks a player uses. It cannot go through the usual cast
// action, since PlayerbotAI::CanCastSpell refuses every cast at an immune target; reach and rank
// are settled by "banish outlived" instead (Felworld).
class CancelBanishAction : public Action
{
public:
    CancelBanishAction(PlayerbotAI* botAI) : Action(botAI, "cancel banish") {}

    bool Execute(Event event) override;
};

// Backs away from a damage-immune enemy player instead of standing in their reach: a bubbled
// paladin still swings, and the bot cannot swing back. Repeats while the enemy stays close; once
// the gap is opened the node goes quiet and the ordinary recovery triggers (food, drink,
// self-heals) take the rest of the window (Felworld).
class ImmunityStandoffAction : public MovementAction
{
public:
    ImmunityStandoffAction(PlayerbotAI* botAI) : MovementAction(botAI, "immunity standoff") {}

    bool isUseful() override;
    bool Execute(Event event) override;
};

#endif
