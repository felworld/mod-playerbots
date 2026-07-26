/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "QuestCompetitionTriggers.h"

#include "GrindTargetValue.h"
#include "Playerbots.h"

bool QuestCompetitionInviteTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.questCompetitionInvite)
        return false;

    if (bot->GetGroup() || bot->InBattleground() || bot->InBattlegroundQueue())
        return false;

    if (!sRandomPlayerbotMgr.IsRandomBot(bot) || botAI->HasActivePlayerMaster())
        return false;

    QuestCompetitionInfo& info = botAI->questCompetitionInfo;
    if (!info.pendingInvite.IsEmpty() || info.active)
        return false;

    GuidVector nearGuids = AI_VALUE(GuidVector, "nearest friendly players");
    for (ObjectGuid const guid : nearGuids)
    {
        Player* player = ObjectAccessor::FindPlayer(guid);
        if (!player || player == bot || !player->IsAlive())
            continue;

        if (player->GetMapId() != bot->GetMapId() || player->IsBeingTeleported())
            continue;

        if (player->GetGroup() || player->isDND())
            continue;

        // One cooldown per player across all bots: in dense areas (starting
        // zones) per-bot cooldowns still add up to an invite barrage.
        if (!sRandomPlayerbotMgr.IsQuestCompetitionInviteAllowed(guid))
            continue;

        if (abs(int32(player->GetLevel()) - int32(bot->GetLevel())) > 4)
            continue;

        if (GET_PLAYERBOT_AI(player))  // only invite real players: the random bot manager
            continue;                  // dismantles bot-led bot groups anyway

        Unit* victim = player->GetVictim();
        if (!victim || !victim->IsCreature() || !victim->IsAlive())
            continue;

        if (!GrindTargetValue::PlayerNeedsCreatureForQuest(bot, victim->GetEntry()))
            continue;

        info.candidate = guid;
        info.candidateEntry = victim->GetEntry();
        return true;
    }

    return false;
}
