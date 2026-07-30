/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "WpvpEmoteAlert.h"

#include "NewRpgInfo.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "Timer.h"
#include "World.h"
#include "WpvpDefense.h"

namespace
{
// How long a sighting stays answerable. Short on purpose: an emote is a
// momentary "look at THAT one", not a standing report - the defense board
// handles long-lived intel.
constexpr uint32 ALERT_TTL_MS = 60 * IN_MILLISECONDS;
}

void NoteTargetedEmoteAtEnemy(Player* emoter, uint32 textEmote, ObjectGuid targetGuid)
{
    if (!sPlayerbotAIConfig.wpvpEmoteAlertEnabled)
        return;

    // Sincere respect gestures are not a call to arms: the same-class truce
    // answers a spared enemy with /salute, and rallying witnesses onto the
    // salutee would invert the gesture. Only unambiguously respectful emotes
    // qualify - a /wave or /smile at an enemy reads just as easily as gank
    // taunting, and still rallies.
    if (textEmote == TEXT_EMOTE_SALUTE || textEmote == TEXT_EMOTE_BOW)
        return;

    if (!emoter || !targetGuid.IsPlayer() || targetGuid == emoter->GetGUID())
        return;

    if (emoter->InBattleground() || emoter->InArena() || emoter->IsGameMaster())
        return;

    // Map-local lookup: the hook runs on the emoter's map-update thread, and
    // an emote's target was on the emoter's screen anyway.
    Player* target = ObjectAccessor::GetPlayer(*emoter, targetGuid);
    if (!target || !target->IsAlive() || target->IsGameMaster())
        return;

    if (target->GetTeamId() == emoter->GetTeamId())
        return;

    // Only enemies the witnesses could actually fight.
    if (!target->IsPvP() && !target->IsFFAPvP())
        return;

    if (sPlayerbotAIConfig.IsInPvpProhibitedZone(target->GetZoneId()))
        return;

    if (!emoter->IsWithinDist(target, sPlayerbotAIConfig.wpvpVisionDistance))
        return;

    WpvpEmoteAlertBoard::instance().Post(emoter, target);
}

void WpvpEmoteAlertBoard::Post(Player* emoter, Player* target)
{
    uint32 now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(now);

    // Refreshing keeps the responded set: emote spam at the same enemy
    // doesn't re-summon witnesses who already answered.
    WpvpEmoteAlertEntry& entry = _entries[target->GetGUID()];
    if (!entry.postedMs)
    {
        entry.target = target->GetGUID();
        entry.postedMs = now;
    }

    entry.emoter = emoter->GetGUID();
    entry.alertedTeam = emoter->GetTeamId();
    entry.zoneId = target->GetZoneId();
    entry.targetPos = WorldPosition(target);
    entry.emoterPos = WorldPosition(emoter);
    entry.targetLevel = target->GetLevel();
    entry.updatedMs = now;
}

bool WpvpEmoteAlertBoard::FindAlertFor(Player* bot, WpvpEmoteAlertEntry& out)
{
    uint32 now = getMSTime();
    float listenRange = sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_TEXTEMOTE);

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(now);

    for (auto const& [guid, entry] : _entries)
    {
        if (entry.alertedTeam != bot->GetTeamId())
            continue;

        if (entry.emoter == bot->GetGUID())
            continue;

        if (entry.responded.count(bot->GetGUID()))
            continue;

        if (bot->GetLevel() + sPlayerbotAIConfig.wpvpDefenseLevelSlack < entry.targetLevel)
            continue;

        // Witnesses only: the same radius real players hear the emote in.
        if (bot->GetMapId() != entry.emoterPos.GetMapId() ||
            WorldPosition(bot).distance(entry.emoterPos) > listenRange)
            continue;

        out = entry;
        return true;
    }

    return false;
}

bool WpvpEmoteAlertBoard::ClaimResponse(ObjectGuid bot, ObjectGuid target)
{
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _entries.find(target);
    if (it == _entries.end())
        return false;

    return it->second.responded.insert(bot).second;
}

void WpvpEmoteAlertBoard::Prune(uint32 now)
{
    for (auto it = _entries.begin(); it != _entries.end();)
    {
        if (now - it->second.updatedMs > ALERT_TTL_MS)
            it = _entries.erase(it);
        else
            ++it;
    }
}

bool WpvpEmoteAlertTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.wpvpEmoteAlertEnabled)
        return false;

    if (bot->InBattleground() || bot->InArena() || bot->GetGroup() || bot->IsInCombat())
        return false;

    if (!sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    if (bot->GetLevel() < sPlayerbotAIConfig.wpvpMinBotLevel)
        return false;

    switch (botAI->rpgInfo.GetStatus())
    {
        case RPG_IDLE:
        case RPG_REST:
        case RPG_WANDER_RANDOM:
        case RPG_WANDER_NPC:
        case RPG_GO_GRIND:
        case RPG_GO_CAMP:
            break;
        case RPG_GO_WPVP:
            // Excursion bots are the prime audience - they're out here exactly
            // to find these fights. Bots already answering a defense call (or
            // an earlier alert) stay on task.
            if (std::get<NewRpgInfo::GoWpvp>(botAI->rpgInfo.data).defend)
                return false;
            break;
        default:
            return false;
    }

    WpvpEmoteAlertEntry entry;
    return WpvpEmoteAlertBoard::instance().FindAlertFor(bot, entry);
}

bool WpvpEmoteAlertAction::Execute(Event /*event*/)
{
    WpvpEmoteAlertEntry entry;
    if (!WpvpEmoteAlertBoard::instance().FindAlertFor(bot, entry))
        return false;

    if (!WpvpEmoteAlertBoard::instance().ClaimResponse(bot->GetGUID(), entry.target))
        return false;

    return StartWpvpDefenseResponse(botAI, entry.zoneId, entry.targetPos, entry.target);
}
