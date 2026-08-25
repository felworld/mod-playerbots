/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DUNGEONHOLDSTRATEGY_H
#define PLAYERBOTS_DUNGEONHOLDSTRATEGY_H

#include "Multiplier.h"
#include "Strategy.h"

class PlayerbotAI;

// In instanced group content nobody but the main tank opens on a mob until the tank actually has it
// (Felworld). Attached to every bot's combat engine; it self-gates on the config and on there being
// a main tank other than the bot to wait for.
class DungeonHoldStrategy : public Strategy
{
public:
    DungeonHoldStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "dungeon hold"; }

private:
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

class DungeonHoldMultiplier : public Multiplier
{
public:
    DungeonHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "dungeon hold") {}

    float GetValue(Action* action) override;
};

#endif
