/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_QUESTCOMPETITIONTRIGGERS_H
#define _PLAYERBOT_QUESTCOMPETITIONTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

// Fires when a nearby ungrouped same-faction player is fighting a creature
// this bot still needs for an in-progress quest - the moment a real player
// would say "want to group for these?". checkInterval 5: spawn competition
// develops over seconds, no need to scan every tick.
// Once the group exists and holds a real player, the same scan recruits other
// bots competing for the same spawns, up to
// AiPlayerbot.QuestCompetitionGroupSize.
class QuestCompetitionInviteTrigger : public Trigger
{
public:
    QuestCompetitionInviteTrigger(PlayerbotAI* botAI) : Trigger(botAI, "quest competition invite", 5) {}

    bool IsActive() override;

private:
    bool CanRecruit();
};

#endif
