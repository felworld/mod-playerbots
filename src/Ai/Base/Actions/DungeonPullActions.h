/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DUNGEONPULLACTIONS_H
#define PLAYERBOTS_DUNGEONPULLACTIONS_H

#include "ChooseTargetActions.h"

class PlayerbotAI;
class PullStrategy;

// The group's main tank opens on the next pack in instanced group content once the whole group is
// ready - with its ranged pull ability when it has one, by walking in otherwise (Felworld).
class DungeonPullAction : public AttackAnythingAction
{
public:
    DungeonPullAction(PlayerbotAI* botAI) : AttackAnythingAction(botAI, "dungeon pull") {}

    std::string const GetTargetName() override { return "dungeon pull target"; }
    bool Execute(Event event) override;
    bool isUseful() override;

    // Everyone the tank would be pulling for is in place and topped up.
    static bool IsGroupReady(PlayerbotAI* botAI);

private:
    // Closes the last stretch to the pull spell's range when the tank leads the group itself.
    bool StepUpToPullRange(Unit* target, PullStrategy* strategy);
};

// In an all-bot group the leader hands leadership to the main tank, so the group follows the puller.
class GiveLeaderToTankAction : public Action
{
public:
    GiveLeaderToTankAction(PlayerbotAI* botAI) : Action(botAI, "give leader to tank") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
