/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NEWRPGACTION_H
#define PLAYERBOTS_NEWRPGACTION_H

#include "Duration.h"
#include "MovementActions.h"
#include "NewRpgBaseAction.h"
#include "NewRpgInfo.h"
#include "NewRpgStrategy.h"
#include "Object.h"
#include "ObjectDefines.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "QuestDef.h"
#include "TravelMgr.h"
#include <string>

class Player;

class TellRpgStatusAction : public NewRpgBaseAction
{
public:
    TellRpgStatusAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "rpg status") {}

    bool Execute(Event event) override;

private:
    static constexpr char const* RPG_STATUS_CHANGED_KEY = "rpg_status_changed";
    static constexpr char const* RPG_STATUS_CHANGED_DEFAULT = "rpg status -> %status";

    void WhisperStatusChange(Player* owner, std::string const& statusName);
};

class StartRpgDoQuestAction : public Action
{
public:
    StartRpgDoQuestAction(PlayerbotAI* botAI) : Action(botAI, "start rpg do quest") {}

    bool Execute(Event event) override;
};

class NewRpgStatusUpdateAction : public NewRpgBaseAction
{
public:
    NewRpgStatusUpdateAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg status update")
    {
        // int statusCount = RPG_STATUS_END - 1;

        // transitionMat.resize(statusCount, std::vector<int>(statusCount, 0));

        // transitionMat[RPG_IDLE][RPG_GO_GRIND] = 20;
        // transitionMat[RPG_IDLE][RPG_GO_CAMP] = 15;
        // transitionMat[RPG_IDLE][RPG_WANDER_NPC] = 30;
        // transitionMat[RPG_IDLE][RPG_DO_QUEST] = 35;
    }
    bool Execute(Event event) override;

protected:
    // static NewRpgStatusTransitionProb transitionMat;
    const int32 statusWanderNpcDuration = 5 * MINUTE  * IN_MILLISECONDS ;
    const int32 statusWanderRandomDuration = 5 * MINUTE  * IN_MILLISECONDS ;
    const int32 statusRestDuration = 30 * IN_MILLISECONDS ;
    const int32 statusDoQuestDuration = 30 * MINUTE  * IN_MILLISECONDS ;
    const int32 statusOutDoorPvPDuration = HOUR * IN_MILLISECONDS ;
    // Travel phase cap for a wpvp excursion (dwell time is rolled per trip)
    const int32 statusGoWpvpTravelDuration = 20 * MINUTE * IN_MILLISECONDS ;
    // Travel phase cap for a duel-spot hangout (dwell time is rolled per trip)
    const int32 statusDuelSpotTravelDuration = 20 * MINUTE * IN_MILLISECONDS ;
    // Cap on casting Teleport: Moonglade (10s cast, may be interrupted a few times)
    const int32 statusGoMoongladeDuration = 2 * MINUTE * IN_MILLISECONDS ;
};

class NewRpgGoGrindAction : public NewRpgBaseAction
{
public:
    NewRpgGoGrindAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg go grind") {}
    bool Execute(Event event) override;
};

class NewRpgGoCampAction : public NewRpgBaseAction
{
public:
    NewRpgGoCampAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg go camp") {}
    bool Execute(Event event) override;
};

class NewRpgWanderRandomAction : public NewRpgBaseAction
{
public:
    NewRpgWanderRandomAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg wander random") {}
    bool Execute(Event event) override;

    // Wandering is the grind state: it only earns its keep while there is
    // something to fight. This long without combat or a grind target means
    // the spot is picked clean (or contested by a crowd) - go idle early and
    // roll a new activity instead of pacing out the full status duration.
    const uint32 wanderDeadSpotTimeout = 60 * IN_MILLISECONDS;
    // Keep the meander within earshot of where it started; the leash steers
    // the drift back rather than teleporting or hard-stopping the bot.
    const float wanderLeashRadius = 150.0f;
};

class NewRpgWanderNpcAction : public NewRpgBaseAction
{
public:
    NewRpgWanderNpcAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg move npcs") {}
    bool Execute(Event event) override;

    const uint32 npcStayTime = 8 * 1000;
};

class NewRpgDoQuestAction : public NewRpgBaseAction
{
public:
    NewRpgDoQuestAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg do quest") {}
    bool Execute(Event event) override;

protected:
    bool DoIncompleteQuest(NewRpgInfo::DoQuest& data);
    bool DoCompletedQuest(NewRpgInfo::DoQuest& data);

    // How long to wait at the turn-in spot before giving up on rewarding
    // (e.g. quests that can only be turned in to gameobjects).
    const uint32 poiStayTime = 5 * 60 * 1000;
    // While an objective is incomplete: how long to work one POI point before
    // moving to another of the quest's points, and how many consecutive
    // stays without a single objective tick before abandoning the quest
    // (3 x 90s keeps the ~5 min terminal timing the old single-stay check had).
    const uint32 poiRotateTime = 90 * 1000;
    const uint8 poiMaxNoProgressStays = 3;
};

class NewRpgTravelFlightAction : public NewRpgBaseAction
{
public:
    NewRpgTravelFlightAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg travel flight") {}
    bool Execute(Event event) override;
};

class NewRpgGoMoongladeAction : public NewRpgBaseAction
{
public:
    NewRpgGoMoongladeAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg go moonglade") {}
    bool Execute(Event event) override;
};

#endif
