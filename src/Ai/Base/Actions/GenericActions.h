/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_GENERICACTIONS_H
#define PLAYERBOTS_GENERICACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "PlayerbotAI.h"

class PlayerbotAI;

class MeleeAction : public AttackAction
{
public:
    MeleeAction(PlayerbotAI* botAI) : AttackAction(botAI, "melee") {}

    std::string const GetTargetName() override { return "current target"; }
    bool isUseful() override;
};

class TogglePetSpellAutoCastAction : public Action
{
public:
    TogglePetSpellAutoCastAction(PlayerbotAI* ai) : Action(ai, "toggle pet spell") {}
    virtual bool Execute(Event event) override;
};

// Sends the pet at the bot's current target ("assist"), instead of letting core PetAI pick a target
// of its own. Registered twice: as "pet attack" (shadowed by the chat command of the same name) and
// as "pet assist", which is the name combat strategies wire to.
class PetAttackAction : public Action
{
public:
    PetAttackAction(PlayerbotAI* ai, std::string const name = "pet attack") : Action(ai, name) {}
    virtual bool Execute(Event event) override;
    bool isUseful() override;
};

class SetPetStanceAction : public Action
{
public:
    SetPetStanceAction(PlayerbotAI* botAI) : Action(botAI, "set pet stance") {}

    bool Execute(Event event) override;
};

#endif
