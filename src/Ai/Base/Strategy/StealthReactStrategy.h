/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_STEALTHREACTSTRATEGY_H
#define _PLAYERBOT_STEALTHREACTSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// The "oh crap" moment when a bot's stealth detection pings someone
// sneaking nearby: freeze, snap to face them, sometimes emote a beat
// later. See StealthReactValues for the detection math.
class StealthReactStrategy : public Strategy
{
public:
    StealthReactStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "stealth react"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
