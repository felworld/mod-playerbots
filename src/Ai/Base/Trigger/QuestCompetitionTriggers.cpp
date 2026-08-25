/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "QuestCompetitionTriggers.h"

#include "GrindTargetValue.h"
#include "Playerbots.h"

// A bot only recruits other bots into a group a real player is already part
// of: bot-only grinding parties have nobody to see them and get dismantled by
// the random bot manager anyway. Party sizes only - the group never converts
// to a raid over this.
bool QuestCompetitionInviteTrigger::CanRecruit()
{
    Group* group = bot->GetGroup();
    if (!group || group->isRaidGroup())
        return false;

    if (group->GetLeaderGUID() != bot->GetGUID())
        return false;

    if (group->GetMembersCount() >= sPlayerbotAIConfig.questCompetitionGroupSize)
        return false;

    for (auto const& slot : group->GetMemberSlots())
        if (IsRealPlayer(ObjectAccessor::FindPlayer(slot.guid)))
            return true;

    return false;
}

bool QuestCompetitionInviteTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.questCompetitionInvite)
        return false;

    if (bot->InBattleground() || bot->InBattlegroundQueue())
        return false;

    if (!sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    QuestCompetitionInfo& info = botAI->questCompetitionInfo;
    if (!info.pendingInvite.IsEmpty())
        return false;

    // Growing an episode group that already has the real player in it, rather
    // than opening one. The master check below doesn't apply: an episode
    // leader has already adopted its partner as master.
    bool const recruiting = info.active;

    if (recruiting)
    {
        if (!CanRecruit())
            return false;
    }
    else
    {
        if (bot->GetGroup())
            return false;

        if (IsRealPlayer(botAI->GetMaster()))
            return false;
    }

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

        if (PlayerbotAI* playerAI = GET_PLAYERBOT_AI(player))
        {
            if (!recruiting)  // an episode is opened by inviting a real player
                continue;

            // Only free-roaming random bots: somebody's altbot is theirs to
            // command, and a selfbot is a person at a keyboard.
            if (!sRandomPlayerbotMgr.IsRandomBot(player) || IsSelfBot(player) || playerAI->IsAltBot())
                continue;

            // They are courting somebody of their own right now.
            if (!playerAI->questCompetitionInfo.pendingInvite.IsEmpty())
                continue;
        }

        for (Unit* fought : PlayerbotAI::GetCreaturesFoughtBy(player))
        {
            if (!GrindTargetValue::PlayerNeedsCreatureForQuest(bot, fought->GetEntry()))
                continue;

            info.candidate = guid;
            info.candidateEntry = fought->GetEntry();
            return true;
        }
    }

    return false;
}
