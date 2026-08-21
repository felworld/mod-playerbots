/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DUNGEONPULLSTRATEGY_H
#define PLAYERBOTS_DUNGEONPULLSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Who opens on the next pack in instanced group content is the main tank, not whichever bot happens
// to hold group leadership (Felworld). Attached to every bot; the actions gate on the role.
class DungeonPullStrategy : public Strategy
{
public:
    DungeonPullStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "dungeon pull"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
