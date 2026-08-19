/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NEWRPGINFO_H
#define PLAYERBOTS_NEWRPGINFO_H

#include "Define.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "Strategy.h"
#include "Timer.h"
#include "TravelMgr.h"

using NewRpgStatusTransitionProb = std::vector<std::vector<int>>;

struct NewRpgInfo
{
    NewRpgInfo() : data(Idle{}) {}
    ~NewRpgInfo() = default;

    // RPG_GO_GRIND
    struct GoGrind
    {
        WorldPosition pos{};
    };
    // RPG_GO_CAMP
    struct GoCamp
    {
        WorldPosition pos{};
    };
    // RPG_WANDER_NPC
    struct WanderNpc
    {
        ObjectGuid npcOrGo{};
        uint32 lastReach{0};
    };
    // RPG_WANDER_RANDOM
    struct WanderRandom
    {
        WanderRandom() = default;
    };
    // RPG_DO_QUEST
    struct DoQuest
    {
        const Quest* quest{nullptr};
        uint32 questId{0};
        int32 objectiveIdx{0};
        WorldPosition pos{};
        uint32 lastReachPOI{0};
    };
    // RPG_TRAVEL_FLIGHT
    struct TravelFlight
    {
        uint32 flightMasterEntry{0};
        WorldPosition flightMasterPos{};
        std::vector<uint32> path;
        bool inFlight{false};
    };
    // RPG_REST
    struct Rest
    {
        Rest() = default;
    };
    // RPG_OUTDOOR_PVP
    struct OutdoorPvP
    {
        ObjectGuid::LowType capturePointSpawnId{0};
    };
    // RPG_GO_WPVP
    struct GoWpvp
    {
        WorldPosition hubPos{};       // the enemy/contested travel hub itself
        WorldPosition anchorPos{};    // dwell spot 40-80yd off the hub
        WorldPosition teleportPos{};  // arrival spot 180-280yd out; bot walks the last leg
        uint32 zoneId{0};
        bool teleported{false};
        uint32 arrivedT{0};           // timestamp of reaching the anchor (0 = still travelling)
        uint32 dwellDuration{0};      // ms to loiter once arrived
        uint32 lastGoadT{0};          // last goad emote timestamp (stealth classes)
        bool raidRolled{false};       // the one bored-raid dice roll has been made
        uint8 deathCount{0};
        bool strategiesApplied{false};
        bool test{false};             // started by "wpvp test": progress logs at INFO
        uint32 lastTestLogT{0};       // throttles repeated test-mode "waiting" logs
        bool defend{false};           // defense response to a callout, not an invasion
        ObjectGuid defendTarget{};    // the reported ganker to hunt (may be empty for hub anchors)
        uint32 departT{0};            // simulated-travel-delay end; 0 = leave immediately
        uint32 defendLastSeenT{0};    // last time the defend target was seen alive in the zone
    };
    // RPG_DUEL_SPOT
    struct DuelSpot
    {
        WorldPosition hubPos{};       // the classic duel field outside the capital gates
        WorldPosition anchorPos{};    // dwell spot sampled a few yards off the hub
        WorldPosition teleportPos{};  // arrival spot ~150yd out; bot walks the last leg
        bool teleported{false};
        uint32 arrivedT{0};           // timestamp of reaching the anchor (0 = still travelling)
        uint32 dwellDuration{0};      // ms to loiter once arrived
        uint32 lastSolicitT{0};       // last "anyone up for a duel?" emote/line
        bool addedStartDuel{false};   // whether WE added "start duel" (vs the AiFactory 25% roll)
    };
    // RPG_GO_MOONGLADE
    struct GoMoonglade
    {
        GoMoonglade() = default;
    };
    struct Idle
    {
    };

    uint32 startT{0};  // start timestamp of the current status

    // Duel-challenge throttling, independent of the current status (the
    // "start duel" strategy fires while roaming as well as at duel spots).
    uint32 lastDuelChallengeT{0};
    ObjectGuid lastDuelChallengeTarget{};

    // MOVE_FAR
    float nearestMoveFarDis{FLT_MAX};
    uint32 stuckTs{0};
    uint32 stuckAttempts{0};
    uint32 lastMoveFarTs{0};  // last MoveFarTo tick; a gap means the bot was interrupted (combat, death, ...)
    WorldPosition moveFarPos;
    // END MOVE_FAR

    using RpgData = std::variant<
        Idle,
        GoGrind,
        GoCamp,
        WanderNpc,
        WanderRandom,
        DoQuest,
        Rest,
        TravelFlight,
        OutdoorPvP,
        GoWpvp,
        DuelSpot,
        GoMoonglade
    >;
    RpgData data;

    NewRpgStatus GetStatus();
    static NewRpgStatus StatusFromString(std::string const& name);
    bool HasStatusPersisted(uint32 maxDuration) { return GetMSTimeDiffToNow(startT) > maxDuration; }
    void ChangeToGoGrind(WorldPosition pos);
    void ChangeToGoCamp(WorldPosition pos);
    void ChangeToWanderNpc();
    void ChangeToWanderRandom();
    void ChangeToDoQuest(uint32 questId, const Quest* quest);
    void ChangeToTravelFlight(uint32 flightMasterEntry, WorldPosition flightMasterPos, std::vector<uint32> path);
    void ChangeToOutdoorPvp(ObjectGuid::LowType capturePointSpawnId = 0);
    void ChangeToGoWpvp(GoWpvp&& wpvp);
    void ChangeToDuelSpot(DuelSpot&& spot);
    void ChangeToGoMoonglade();
    void ChangeToRest();
    void ChangeToIdle();
    bool CanChangeTo(NewRpgStatus status);
    void Reset();
    void SetMoveFarTo(WorldPosition pos);
    std::string ToString();
};

struct NewRpgStatistic
{
    uint32 questAccepted{0};
    uint32 questCompleted{0};
    uint32 questAbandoned{0};
    uint32 questRewarded{0};
    uint32 questDropped{0};
    NewRpgStatistic operator+(const NewRpgStatistic& other) const
    {
        NewRpgStatistic result;
        result.questAccepted = this->questAccepted + other.questAccepted;
        result.questCompleted = this->questCompleted + other.questCompleted;
        result.questAbandoned = this->questAbandoned + other.questAbandoned;
        result.questRewarded = this->questRewarded + other.questRewarded;
        result.questDropped = this->questDropped + other.questDropped;
        return result;
    }
    NewRpgStatistic& operator+=(const NewRpgStatistic& other)
    {
        this->questAccepted += other.questAccepted;
        this->questCompleted += other.questCompleted;
        this->questAbandoned += other.questAbandoned;
        this->questRewarded += other.questRewarded;
        this->questDropped += other.questDropped;
        return *this;
    }
};

#endif
