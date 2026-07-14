/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_BYSTANDERASSISTACTIONS_H
#define _PLAYERBOT_BYSTANDERASSISTACTIONS_H

#include "AttackAction.h"
#include "ReachTargetActions.h"

class PlayerbotAI;

// Rescue paths for "bystander in distress": healer classes heal the victim
// (running into heal range first if needed); everyone else charges the
// creature attacking them. One trigger drives all three - the engine falls
// through on isUseful().

class BystanderHealAction : public Action
{
public:
    BystanderHealAction(PlayerbotAI* botAI) : Action(botAI, "bystander heal") {}

    std::string const GetTargetName() override { return "bystander to assist"; }
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ReachBystanderToAssistAction : public ReachTargetAction
{
public:
    ReachBystanderToAssistAction(PlayerbotAI* botAI);

    std::string const GetTargetName() override { return "bystander to assist"; }
    bool isUseful() override;
};

class AttackBystanderAttackerAction : public AttackAction
{
public:
    AttackBystanderAttackerAction(PlayerbotAI* botAI) : AttackAction(botAI, "attack bystander attacker") {}

    std::string const GetTargetName() override { return "bystander attacker"; }
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
