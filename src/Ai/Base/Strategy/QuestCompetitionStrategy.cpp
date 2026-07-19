/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "QuestCompetitionStrategy.h"

#include "Playerbots.h"

void QuestCompetitionStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Relevance 60: above routine non-combat actions (grind, loot, rpg all
    // run below ~40) so a spotted competitor actually gets invited, but well
    // below combat and rescue relevances.
    triggers.push_back(new TriggerNode("quest competition invite", {
        NextAction("quest competition invite", 60.0f) }));
}
