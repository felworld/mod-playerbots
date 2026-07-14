/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_BYSTANDERASSISTSTRATEGY_H
#define _PLAYERBOT_BYSTANDERASSISTSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Non-combat strategy for solo random bots: rescue nearby non-group players
// (real players and bots) who look like they're about to die.
class BystanderAssistStrategy : public Strategy
{
public:
    BystanderAssistStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "bystander assist"; }
};

#endif
