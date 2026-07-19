/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_QUESTCOMPETITIONACTIONS_H
#define _PLAYERBOT_QUESTCOMPETITIONACTIONS_H

#include "InviteToGroupAction.h"

class PlayerbotAI;

// Silently invites the competing player found by QuestCompetitionInviteTrigger
// and opens a quest-competition episode (see PlayerbotAI::UpdateQuestCompetition).
class QuestCompetitionInviteAction : public InviteToGroupAction
{
public:
    QuestCompetitionInviteAction(PlayerbotAI* botAI) : InviteToGroupAction(botAI, "quest competition invite") {}

    bool Execute(Event event) override;
};

#endif
