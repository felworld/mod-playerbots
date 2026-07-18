/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_SOCIALBUFFSTRATEGY_H
#define _PLAYERBOT_SOCIALBUFFSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Courtesy behaviour for idle bots: buff passersby, buff back whoever buffs
// you, /thank whoever heals you. Non-combat only; see SocialBuffValues for
// the PvP-flagging and target-selection rules.
class SocialBuffStrategy : public Strategy
{
public:
    SocialBuffStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "social buff"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
