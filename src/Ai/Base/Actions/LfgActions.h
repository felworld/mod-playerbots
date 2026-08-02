/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_LFGACTIONS_H
#define PLAYERBOTS_LFGACTIONS_H

#include "InventoryAction.h"

class PlayerbotAI;

class LfgJoinAction : public InventoryAction
{
public:
    LfgJoinAction(PlayerbotAI* botAI, std::string const name = "lfg join") : InventoryAction(botAI, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

protected:
    bool JoinLFG();
    uint32 GetRoles();
};

class LfgAcceptAction : public LfgJoinAction
{
public:
    LfgAcceptAction(PlayerbotAI* botAI) : LfgJoinAction(botAI, "lfg accept") {}

    bool Execute(Event event) override;
    bool isUseful() override { return true; }

protected:
    bool AcceptProposal(uint32 proposalId);
};

class LfgRoleCheckAction : public LfgJoinAction
{
public:
    LfgRoleCheckAction(PlayerbotAI* botAI) : LfgJoinAction(botAI, "lfg role check") {}

    bool Execute(Event event) override;
    bool isUseful() override { return true; }
};

class LfgLeaveAction : public Action
{
public:
    LfgLeaveAction(PlayerbotAI* botAI) : Action(botAI, "lfg leave") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class LfgTeleportAction : public Action
{
public:
    LfgTeleportAction(PlayerbotAI* botAI) : Action(botAI, "lfg teleport") {}

    bool Execute(Event event) override;
};

// Retries the port into the dungeon for a bot that is still outside it. The core only teleports the
// group once (LFGMgr::MakeNewGroup) and drops the bot silently when that attempt is refused.
class LfgEnterDungeonAction : public Action
{
public:
    LfgEnterDungeonAction(PlayerbotAI* botAI, std::string const name = "lfg enter dungeon")
        : Action(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;

protected:
    void ClearTeleportBlockers();
};

// SMSG_LFG_TELEPORT_DENIED handler: makes the failure visible and pre-clears whatever we can, so the
// next "lfg outside dungeon" retry has a chance of succeeding.
class LfgTeleportDeniedAction : public LfgEnterDungeonAction
{
public:
    LfgTeleportDeniedAction(PlayerbotAI* botAI) : LfgEnterDungeonAction(botAI, "lfg teleport denied") {}

    bool Execute(Event event) override;
    bool isUseful() override { return true; }
};

#endif
