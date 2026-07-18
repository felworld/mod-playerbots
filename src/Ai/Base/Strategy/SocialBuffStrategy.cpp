/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "SocialBuffStrategy.h"

#include "Playerbots.h"

void SocialBuffStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Relevance 85-87: above routine idle behaviour (grind, rpg, emote all
    // run below ~40) but under bystander assist (90) - a rescue outranks
    // courtesy - and under 100 so bots with no real player nearby (Engine
    // skips <100 nodes while "minimal") don't spend their throttled ticks
    // being polite to nobody. Reactions outrank the walk-up buff: answer
    // debts before making new friends.
    triggers.push_back(new TriggerNode("healed by friendly", {
        NextAction("thank healer", 87.0f) }));
    triggers.push_back(new TriggerNode("buffed by friendly", {
        NextAction("buff back", 86.0f) }));
    triggers.push_back(new TriggerNode("passerby to buff", {
        NextAction("buff passerby", 85.0f) }));
}
