/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "QuestCompetitionActions.h"

#include "Event.h"
#include "Playerbots.h"

bool QuestCompetitionInviteAction::Execute(Event /*event*/)
{
    QuestCompetitionInfo& info = botAI->questCompetitionInfo;
    ObjectGuid candidateGuid = info.candidate;
    uint32 candidateEntry = info.candidateEntry;
    info.candidate.Clear();
    info.candidateEntry = 0;

    if (candidateGuid.IsEmpty() || !candidateEntry)
        return false;

    Player* player = ObjectAccessor::FindPlayer(candidateGuid);
    if (!player || player->GetGroup() || player->GetMapId() != bot->GetMapId())
        return false;

    // Atomic re-check-and-record: another bot's trigger (possibly on another
    // map thread) may have picked the same player before either action ran.
    // The cooldown starts at the attempt, so a decline isn't followed by
    // pestering.
    if (!sRandomPlayerbotMgr.TryRecordQuestCompetitionInvite(candidateGuid))
        return false;

    if (!Invite(bot, player))
        return false;

    info.pendingInvite = candidateGuid;
    info.pendingSince = time(nullptr);
    // Recruits joining an episode already in progress extend the shared
    // objectives instead of resetting them.
    if (!info.active)
        info.entries.clear();

    info.entries.insert(candidateEntry);
    return true;
}
