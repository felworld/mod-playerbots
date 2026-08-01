/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CHESTROLLMGR_H
#define PLAYERBOTS_CHESTROLLMGR_H

#include "ObjectGuid.h"

#include <map>
#include <mutex>

class GameObject;
class Group;
class Player;

// Arbitrates world chests between grouped bots: instead of the first bot in
// range ninja-looting, every bot that could open the chest performs a visible
// /roll (1-100) and only the highest roller opens it. A real player in the
// group can join the contest by typing /roll during the window; if the player
// wins, the bots leave the chest alone.
class ChestRollMgr
{
public:
    static ChestRollMgr* instance();

    // Gate called from loot-target selection. Returns true when the bot may
    // loot the gameobject now; for an eligible chest this starts or joins a
    // roll-off (side effect: the bot /rolls once) and returns false while
    // the contest is undecided or was lost to someone else.
    bool MayLoot(Player* bot, GameObject* go, uint32 lootSkillId);

    // Record a real player's /roll observed via MSG_RANDOM_ROLL so it
    // competes in any open contest of the player's group.
    void RecordPlayerRoll(Player* roller, uint32 rollMin, uint32 rollMax, uint32 result);

private:
    struct RollEntry
    {
        uint32 value = 0;
        uint32 order = 0;  // ties go to the earliest roll
    };

    struct Session
    {
        ObjectGuid groupGuid;
        ObjectGuid winnerGuid;
        time_t rollsCloseAt = 0;
        time_t claimExpiresAt = 0;
        bool decided = false;
        uint32 nextOrder = 0;
        std::map<ObjectGuid, RollEntry> rolls;
    };

    static bool IsRollableChest(GameObject* go, uint32 lootSkillId);
    static bool GroupHasRealPlayer(Group* group);
    static void Decide(Session& session, time_t now);
    void Prune(time_t now);

    std::mutex _mutex;
    std::map<ObjectGuid, Session> _sessions;  // keyed by chest guid
    time_t _lastPrune = 0;
};

#define sChestRollMgr ChestRollMgr::instance()

#endif
