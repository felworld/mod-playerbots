/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DuelStrategy.h"

void DuelStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    PassThroughStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode("duel requested", { NextAction("accept duel", relevance) }));
    triggers.push_back(
        new TriggerNode("no attackers", { NextAction("attack duel opponent", 70.0f) }));
}

DuelStrategy::DuelStrategy(PlayerbotAI* botAI) : PassThroughStrategy(botAI) {}

void StartDuelStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Above the "new rpg status update" default action (11.0f) so a bot
    // that can challenge does so before wandering off to a new status.
    triggers.push_back(
        new TriggerNode("start duel possible", { NextAction("start duel", 12.0f) }));
}

StartDuelStrategy::StartDuelStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}
