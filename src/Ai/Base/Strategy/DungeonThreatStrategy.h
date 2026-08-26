/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DUNGEONTHREATSTRATEGY_H
#define PLAYERBOTS_DUNGEONTHREATSTRATEGY_H

#include "Multiplier.h"
#include "Strategy.h"

class PlayerbotAI;

// Threat discipline for instanced group content (Felworld). A bot that is not the main tank stops
// offense against a mob once its own threat closes on the tank's, the way a player watching Omen
// stops. Attached by the generic dungeon fallback in PlayerbotAI::ApplyInstanceStrategies, so it is
// only ever live on maps with no bespoke instance pack; it self-gates on instanced group content and
// on there being a live main tank other than the bot.
//
// The upstream "threat" strategy is the same idea with two faults we cannot live with: it throttles
// heals and buffs (upstream tags every heal ActionThreatType::Aoe), and its threat ratio divides by
// the tank's threat without checking it for zero. This one filters offense by action family and
// computes the ratio itself.
class DungeonThreatStrategy : public Strategy
{
public:
    DungeonThreatStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "dungeon threat"; }

private:
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

class DungeonThreatMultiplier : public Multiplier
{
public:
    DungeonThreatMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "dungeon threat") {}

    float GetValue(Action* action) override;
};

#endif
