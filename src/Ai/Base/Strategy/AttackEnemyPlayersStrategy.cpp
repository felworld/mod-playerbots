/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "AttackEnemyPlayersStrategy.h"

#include "Playerbots.h"

void AttackEnemyPlayersStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("enemy player near",
                                       { NextAction("attack enemy player", 55.0f) }));
    // "pvp" is on every non-BG bot, so this is where defenders shout about
    // invaders in LocalDefense (heavily throttled in WpvpCalloutThrottle).
    triggers.push_back(new TriggerNode("wpvp defense callout",
                                       { NextAction("wpvp defense callout", 40.0f) }));
    // Uncontested killing sprees get one WorldDefense shout, claimed by an
    // eyewitness (a victim, or a bot with the ganker on its screen).
    triggers.push_back(new TriggerNode("wpvp escalation callout",
                                       { NextAction("wpvp escalation callout", 39.0f) }));
    // Idle bots elsewhere may answer a callout and travel in to defend.
    triggers.push_back(new TriggerNode("wpvp defense response",
                                       { NextAction("wpvp defense response", 38.0f) }));
    // And a ganker whose spree collapses under outside help may pull idle
    // faction-mates in as reinforcements (no chat - assumed backchannel).
    triggers.push_back(new TriggerNode("wpvp reinforce",
                                       { NextAction("wpvp reinforce", 37.0f) }));
}
