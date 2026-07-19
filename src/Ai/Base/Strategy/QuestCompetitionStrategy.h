/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_QUESTCOMPETITIONSTRATEGY_H
#define _PLAYERBOT_QUESTCOMPETITIONSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Non-combat strategy for solo random bots: when competing with a nearby
// player for the same quest mobs, invite them to a temporary group. The
// rest of the episode (strategy switch on accept, thanks + leave once the
// shared objectives are done) runs in PlayerbotAI::UpdateQuestCompetition.
class QuestCompetitionStrategy : public Strategy
{
public:
    QuestCompetitionStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "quest competition"; }
};

#endif
