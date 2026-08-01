/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ChestRollMgr.h"

#include "GameObject.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"

namespace
{
constexpr time_t ROLL_WINDOW_SECS = 5;    // rolls accepted this long after the chest is spotted
constexpr time_t CLAIM_WINDOW_SECS = 60;  // winner's exclusive window before the chest frees up
constexpr time_t PRUNE_INTERVAL_SECS = 60;
constexpr time_t PRUNE_AFTER_SECS = 300;  // forget sessions this long after they conclude
}

ChestRollMgr* ChestRollMgr::instance()
{
    static ChestRollMgr instance;
    return &instance;
}

bool ChestRollMgr::IsRollableChest(GameObject* go, uint32 lootSkillId)
{
    if (go->GetGoType() != GAMEOBJECT_TYPE_CHEST)
        return false;

    GameObjectTemplate const* goInfo = go->GetGOInfo();
    if (!goInfo)
        return false;

    // Chests with group loot rules already run real need/greed rolls on
    // their contents, and quest chests are lootable by everyone on the
    // quest - neither needs arbitration.
    if (goInfo->chest.groupLootRules || goInfo->chest.questId)
        return false;

    // Gathering nodes are chests too; those stay first-come-first-served.
    switch (lootSkillId)
    {
        case SKILL_HERBALISM:
        case SKILL_MINING:
        case SKILL_FISHING:
            return false;
        default:
            return true;
    }
}

bool ChestRollMgr::GroupHasRealPlayer(Group* group)
{
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsInWorld() && !GET_PLAYERBOT_AI(member))
            return true;
    }

    return false;
}

bool ChestRollMgr::MayLoot(Player* bot, GameObject* go, uint32 lootSkillId)
{
    if (!sPlayerbotAIConfig.chestRollEnable)
        return true;

    if (!IsRollableChest(go, lootSkillId))
        return true;

    Group* group = bot->GetGroup();
    if (!group || group->isBGGroup() || group->isBFGroup())
        return true;

    if (!GroupHasRealPlayer(group))
        return true;

    time_t now = time(nullptr);
    ObjectGuid botGuid = bot->GetGUID();

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(now);

    auto [it, created] = _sessions.try_emplace(go->GetGUID());
    Session& session = it->second;
    if (created)
    {
        session.groupGuid = group->GetGUID();
        session.rollsCloseAt = now + ROLL_WINDOW_SECS;
    }
    else if (session.groupGuid != group->GetGUID())
        return true;  // contested by another group - no arbitration across groups

    if (!session.decided)
    {
        if (now <= session.rollsCloseAt && session.rolls.find(botGuid) == session.rolls.end())
        {
            // Safe under the lock: the group broadcast reaches the other
            // bots' packet handlers synchronously, but those discard
            // playerbot rollers before calling back into this manager.
            uint32 value = bot->DoRandomRoll(1, 100);
            session.rolls[botGuid] = { value, session.nextOrder++ };
        }

        if (now <= session.rollsCloseAt)
            return false;

        Decide(session, now);
    }

    if (session.winnerGuid == botGuid)
        return true;

    // A winner who never collects (walked away, died, real player who
    // changed their mind) eventually frees the chest for anyone.
    return now > session.claimExpiresAt;
}

void ChestRollMgr::RecordPlayerRoll(Player* roller, uint32 rollMin, uint32 rollMax, uint32 result)
{
    if (!sPlayerbotAIConfig.chestRollEnable)
        return;

    if (rollMin != 1 || rollMax != 100)
        return;

    Group* group = roller->GetGroup();
    if (!group)
        return;

    ObjectGuid groupGuid = group->GetGUID();
    ObjectGuid rollerGuid = roller->GetGUID();
    time_t now = time(nullptr);

    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& [chestGuid, session] : _sessions)
    {
        if (session.decided || session.groupGuid != groupGuid || now > session.rollsCloseAt)
            continue;

        if (session.rolls.find(rollerGuid) != session.rolls.end())
            continue;

        session.rolls[rollerGuid] = { result, session.nextOrder++ };
    }
}

void ChestRollMgr::Decide(Session& session, time_t now)
{
    session.decided = true;
    session.claimExpiresAt = now + CLAIM_WINDOW_SECS;

    RollEntry best;
    for (auto const& [rollerGuid, entry] : session.rolls)
    {
        if (session.winnerGuid && (entry.value < best.value || (entry.value == best.value && entry.order > best.order)))
            continue;

        session.winnerGuid = rollerGuid;
        best = entry;
    }
}

void ChestRollMgr::Prune(time_t now)
{
    if (now - _lastPrune < PRUNE_INTERVAL_SECS)
        return;

    _lastPrune = now;
    for (auto it = _sessions.begin(); it != _sessions.end();)
    {
        Session const& session = it->second;
        time_t idleSince = session.decided ? session.claimExpiresAt : session.rollsCloseAt;
        if (now > idleSince + PRUNE_AFTER_SECS)
            it = _sessions.erase(it);
        else
            ++it;
    }
}
