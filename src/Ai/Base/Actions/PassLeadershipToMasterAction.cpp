/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PassLeadershipToMasterAction.h"
#include "Event.h"
#include "PlayerbotOperations.h"
#include "PlayerbotWorldThreadProcessor.h"

bool PassLeadershipToMasterAction::Execute(Event /*event*/)
{
    if (Player* master = GetMaster())
        if (master && master != bot && bot->GetGroup() && bot->GetGroup()->IsMember(master->GetGUID()))
        {
            // A random bot sheds its leader kit (grind, rpg, ...) once it is no longer leader. That has
            // to happen after the queued leader change lands: resetting here, while the bot is still
            // leader, just re-added the kit - and nothing removed it later (Felworld).
            auto setLeaderOp = std::make_unique<GroupSetLeaderOperation>(bot->GetGUID(), master->GetGUID(),
                                                                         sRandomPlayerbotMgr.IsRandomBot(bot));
            PlayerbotWorldThreadProcessor::instance().QueueOperation(std::move(setLeaderOp));

            if (!message.empty())
                botAI->TellMasterNoFacing(message);

            return true;
        }

    return false;
}

bool PassLeadershipToMasterAction::isUseful()
{
    return botAI->IsAltBot() && bot->GetGroup() && bot->GetGroup()->IsLeader(bot->GetGUID());
}

bool GiveLeaderAction::isUseful()
{
    return IsRealPlayer(botAI->GetMaster()) && bot->GetGroup() && bot->GetGroup()->IsLeader(bot->GetGUID());
}
