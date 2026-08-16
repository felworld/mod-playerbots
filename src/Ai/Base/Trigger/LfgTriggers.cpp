/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "LfgTriggers.h"
#include "Group.h"
#include "LFGMgr.h"
#include "Playerbots.h"

using namespace lfg;

bool LfgProposalActiveTrigger::IsActive() { return AI_VALUE(uint32, "lfg proposal"); }

bool LfgOutsideDungeonTrigger::IsActive()
{
    Group* group = bot->GetGroup();
    if (!group || !group->isLFGGroup())
        return false;

    if (!bot->IsInWorld() || !bot->IsAlive() || bot->IsBeingTeleported())
        return false;

    // Only while the group is actually running the dungeon: not while it is still forming
    // (rolecheck / queued / proposal) and not once it has been completed.
    if (sLFGMgr->GetState(group->GetGUID()) != LFG_STATE_DUNGEON)
        return false;

    uint32 mapId = sLFGMgr->GetDungeonMapId(group->GetGUID());
    if (!mapId || bot->GetMapId() == mapId)
        return false;

    // Never drag a bot away from a real player who deliberately stayed outside - only follow a
    // master who is already inside.
    if (botAI->HasGameClientMaster())
    {
        Player* master = botAI->GetMaster();
        if (!master || !master->IsInWorld() || master->GetMapId() != mapId)
            return false;
    }

    return true;
}

bool UnknownDungeonTrigger::IsActive()
{
    return IsRealPlayer(botAI->GetMaster()) && botAI->GetMaster() && botAI->GetMaster()->IsInWorld() &&
           botAI->GetMaster()->GetMap()->IsDungeon() && bot->GetMapId() == botAI->GetMaster()->GetMapId();
}
