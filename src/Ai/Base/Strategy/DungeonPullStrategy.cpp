/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DungeonPullStrategy.h"
#include "Playerbots.h"

void DungeonPullStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Same relevance as grind's "attack anything": above follow, below loot.
    triggers.push_back(new TriggerNode("no target", { NextAction("dungeon pull", 4.0f) }));

    // All-bot groups follow their leader, so the tank has to be the leader for the group to advance.
    triggers.push_back(new TriggerNode("often", { NextAction("give leader to tank", 4.0f) }));
}
