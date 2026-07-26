/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ResetAiAction.h"

#include "Event.h"
#include "Group.h"
#include "ObjectGuid.h"
#include "PlayerbotRepository.h"
#include "Playerbots.h"
#include "WorldPacket.h"

bool ResetAiAction::Execute(Event event)
{
    // Packet-triggered resets ("group list", "group set leader") are only
    // meant for the bot dropping out of its group. There is no single core
    // packet covering every removal path (disband, kick, leave), and the
    // closest proxy - an empty SMSG_GROUP_LIST - is also sent to the leader
    // on group creation. So ask the live state instead: RemoveMember and
    // Disband null the bot's group pointer before this action runs.
    if (!event.getPacket().empty() && bot->GetGroup())
        return false;

    if (Player* master = botAI->GetMaster())
    {
        Group* botGroup = bot->GetGroup();
        Group* masterGroup = master->GetGroup();
        if (botGroup && (!masterGroup || masterGroup != botGroup))
            botAI->SetMaster(nullptr);
    }
    if (sRandomPlayerbotMgr.IsRandomBot(bot) && !bot->InBattleground())
    {
        if (bot->GetGroup() && (!botAI->GetMaster() || GET_PLAYERBOT_AI(botAI->GetMaster())))
        {
            if (Player* newMaster = botAI->FindNewMaster())
                botAI->SetMaster(newMaster);
        }
    }
    PlayerbotRepository::instance().Reset(botAI);
    botAI->ResetStrategies(false);

    // Reply only to the explicit "reset botAI" chat command (empty packet);
    // packet-triggered resets are mechanical, and TellMaster would leak this
    // internals line into party chat.
    if (event.getPacket().empty())
        botAI->TellMaster("AI was reset to defaults");

    return true;
}
