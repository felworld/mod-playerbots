#ifndef PLAYERBOTS_WPVPSTRATEGY_H
#define PLAYERBOTS_WPVPSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Non-combat additions active only while a bot dwells on a world-PvP
// excursion (applied/removed by NewRpgGoWpvpAction / EndWpvpExcursion):
// stealth classes goad unflagged enemies, Night Elves of other classes
// Shadowmeld while loitering.
class WpvpExcursionStrategy : public Strategy
{
public:
    WpvpExcursionStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "wpvp"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
