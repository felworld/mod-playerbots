/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_SOCIALBUFFACTIONS_H
#define _PLAYERBOT_SOCIALBUFFACTIONS_H

#include "Action.h"

class PlayerbotAI;
class Unit;

// Courtesy paths for the "social buff" strategy: idle buff-capable bots bless
// passersby, return the favour when blessed, and thank strangers who heal
// them. All fire only out of combat and never at the cost of PvP-flagging an
// unflagged bot.

class BuffPasserbyAction : public Action
{
public:
    BuffPasserbyAction(PlayerbotAI* botAI) : Action(botAI, "buff passerby") {}

    std::string const GetTargetName() override { return "passerby to buff"; }
    bool Execute(Event event) override;
    bool isUseful() override;
};

class BuffBackAction : public Action
{
public:
    BuffBackAction(PlayerbotAI* botAI) : Action(botAI, "buff back") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class ThankHealerAction : public Action
{
public:
    ThankHealerAction(PlayerbotAI* botAI) : Action(botAI, "thank healer") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
