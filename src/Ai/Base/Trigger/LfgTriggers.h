/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_LFGTRIGGERS_H
#define PLAYERBOTS_LFGTRIGGERS_H

#include "Trigger.h"
#include "WorldPacketTrigger.h"

class PlayerbotAI;

// A deferred proposal only lives for LFG_TIME_PROPOSAL (40s), so poll often enough that a bot which
// was busy when the proposal arrived still gets several chances to answer before it expires.
class LfgProposalActiveTrigger : public Trigger
{
public:
    LfgProposalActiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, "lfg proposal active", 2) {}

    bool IsActive() override;
};

// The core teleports the group into the dungeon exactly once, when the group is formed, and silently
// gives up if the bot happened to be falling / in combat / dead / in a vehicle at that moment. Poll
// for "in an LFG group that is running a dungeon, but not on the dungeon map" so we can retry.
class LfgOutsideDungeonTrigger : public Trigger
{
public:
    LfgOutsideDungeonTrigger(PlayerbotAI* botAI) : Trigger(botAI, "lfg outside dungeon", 5) {}

    bool IsActive() override;
};

// SMSG_LFG_TELEPORT_DENIED - the core telling us a teleport attempt failed and why.
class LfgTeleportDeniedTrigger : public WorldPacketTrigger
{
public:
    LfgTeleportDeniedTrigger(PlayerbotAI* botAI) : WorldPacketTrigger(botAI, "lfg teleport denied") {}
};

class UnknownDungeonTrigger : public Trigger
{
public:
    UnknownDungeonTrigger(PlayerbotAI* botAI) : Trigger(botAI, "unknown dungeon", 20 * 2000) {}

    bool IsActive() override;
};

#endif
