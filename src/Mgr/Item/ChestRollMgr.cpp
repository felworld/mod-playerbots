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

    // Only a chest that is emptied once is worth contesting. A chest that
    // is not consumed by looting (chest.consumable == 0) goes straight back
    // to GO_READY when the looter is done, and one with a restock timer
    // (chest.chestRestockTime) re-arms after that many seconds; either way
    // the core rolls fresh personal loot for the next player who opens it
    // (Player::SendLoot -> Loot::FillLoot with personal = true), so every
    // group member can take their own copy and a roll decides nothing.
    // Scarlet Monastery's "Red Rocket" (entry 103820, chestRestockTime 1)
    // is one of these - bots rolled over its Red Fireworks Rocket, which
    // everyone standing there could simply loot. Ambiguous chests belong on
    // this side of the line: a skipped roll is invisible, a pointless
    // roll-off is a robotic tell.
    if (!goInfo->chest.consumable || goInfo->chest.chestRestockTime)
        return false;

    // Opening some chests does more to the world than hand over loot: a
    // linked trap fires, a script runs, a looted-event goes off. Those are
    // props and set pieces rather than treasure - Zul'Farrak's Shallow
    // Graves (entries 128308/128403, forty of them across the instance,
    // each summoning a pack of Zul'Farrak Zombies on open and despawning
    // behind them) are the case that named this check. What is being
    // contested there isn't loot, it's who springs the trap, and announcing
    // a roll-off for it is a robotic tell. Across the 3.3.5a chest
    // templates the line catches trapped herb nodes, quest pickups and
    // event props; no ordinary treasure chest carries any of the three.
    if (goInfo->chest.linkedTrapId || goInfo->chest.eventId || !goInfo->AIName.empty() || goInfo->ScriptId)
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

Group* ChestRollMgr::ArbitratingGroup(Player* bot, GameObject* go, uint32 lootSkillId)
{
    if (!sPlayerbotAIConfig.chestRollEnable)
        return nullptr;

    if (!IsRollableChest(go, lootSkillId))
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group || group->isBGGroup() || group->isBFGroup())
        return nullptr;

    if (!GroupHasRealPlayer(group))
        return nullptr;

    return group;
}

bool ChestRollMgr::GroupHasOpenContest(ObjectGuid groupGuid, time_t now) const
{
    for (auto const& [chestGuid, session] : _sessions)
        if (!session.decided && session.groupGuid == groupGuid && now <= session.rollsCloseAt)
            return true;

    return false;
}

bool ChestRollMgr::IsBlocked(Player* bot, GameObject* go, uint32 lootSkillId)
{
    Group* group = ArbitratingGroup(bot, go, lootSkillId);
    if (!group)
        return false;

    time_t now = time(nullptr);
    ObjectGuid botGuid = bot->GetGUID();

    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _sessions.find(go->GetGUID());
    if (it == _sessions.end())
        return false;  // no contest yet - still a candidate, MayLoot may start one

    Session const& session = it->second;
    if (session.groupGuid != group->GetGUID())
        return false;  // contested by another group - no arbitration across groups

    // An undecided contest past its window is a candidate: whoever picks the
    // chest next settles it in MayLoot, so a winner who wandered off can't
    // leave it in limbo.
    if (!session.decided)
        return now <= session.rollsCloseAt && session.rolls.find(botGuid) != session.rolls.end();

    return session.winnerGuid != botGuid && now <= session.claimExpiresAt;
}

bool ChestRollMgr::MayLoot(Player* bot, GameObject* go, uint32 lootSkillId)
{
    Group* group = ArbitratingGroup(bot, go, lootSkillId);
    if (!group)
        return true;

    time_t now = time(nullptr);
    ObjectGuid botGuid = bot->GetGUID();
    ObjectGuid groupGuid = group->GetGUID();

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(now);

    auto it = _sessions.find(go->GetGUID());
    if (it == _sessions.end())
    {
        // Contests are serialized per group: while one is open, nobody
        // opens another, so the rolls in chat always refer to the one
        // announced chest (and the human's /roll, recorded into every open
        // session of the group, can only mean one thing). This bot simply
        // retries on a later loot pass; the wait is at most one window.
        if (GroupHasOpenContest(groupGuid, now))
            return false;

        Session& fresh = _sessions[go->GetGUID()];
        fresh.groupGuid = groupGuid;
        fresh.rollsCloseAt = now + ROLL_WINDOW_SECS;
        // The opener anchors the contest for any humans watching: without
        // this line the rolls that follow are uninterpretable, and it
        // doubles as the invitation to type /roll and join in. Sent under
        // the lock, but chat delivery never calls back into this manager.
        if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot))
        {
            std::string announce = "Rolling for " + go->GetGOInfo()->name;
            group->isRaidGroup() ? botAI->SayToRaid(announce) : botAI->SayToParty(announce);
        }
        // Safe under the lock: the group broadcast reaches the other bots'
        // packet handlers synchronously, but those discard playerbot
        // rollers before calling back into this manager.
        fresh.rolls[botGuid] = { bot->DoRandomRoll(1, 100), fresh.nextOrder++ };
        return false;
    }

    Session& session = it->second;
    if (session.groupGuid != groupGuid)
        return true;  // contested by another group - no arbitration across groups

    if (!session.decided)
    {
        if (now <= session.rollsCloseAt)
        {
            if (session.rolls.find(botGuid) == session.rolls.end())
                session.rolls[botGuid] = { bot->DoRandomRoll(1, 100), session.nextOrder++ };

            return false;
        }

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
