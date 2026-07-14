/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "BystanderAssistStrategy.h"

#include "Playerbots.h"

void BystanderAssistStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Relevance 90: outranks every routine non-combat action (grind, loot,
    // rpg, emote all run below ~40) but stays under 100 on purpose - bots
    // throttled for having no real player nearby (Engine skips <100 nodes
    // while "minimal") don't burn scans on rescues nobody would see. A
    // distressed real player makes nearby bots active by definition.
    triggers.push_back(new TriggerNode("bystander in distress", {
        NextAction("bystander heal", 90.0f),
        NextAction("reach bystander to assist", 89.0f),
        NextAction("attack bystander attacker", 88.0f) }));
}
