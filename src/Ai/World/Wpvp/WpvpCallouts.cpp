#include "WpvpCallouts.h"

#include "DBCStores.h"
#include "WpvpDefense.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Random.h"
#include "RandomPlayerbotMgr.h"
#include "StringFormat.h"
#include "Timer.h"

namespace
{
uint64 ZoneKey(TeamId team, uint32 zoneId) { return (uint64(team) << 32) | zoneId; }
}

bool WpvpCalloutThrottle::CanReport(TeamId team, uint32 zoneId, ObjectGuid attacker)
{
    std::lock_guard<std::mutex> lock(_mutex);
    return Check(ZoneKey(team, zoneId), attacker, getMSTime());
}

bool WpvpCalloutThrottle::TryReport(TeamId team, uint32 zoneId, ObjectGuid attacker)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    if (!Check(ZoneKey(team, zoneId), attacker, now))
        return false;

    _lastZoneCallout[ZoneKey(team, zoneId)] = now;
    _lastAttackerCallout[attacker] = now;
    Prune(now);
    return true;
}

bool WpvpCalloutThrottle::Check(uint64 zoneKey, ObjectGuid attacker, uint32 now) const
{
    auto zoneIt = _lastZoneCallout.find(zoneKey);
    if (zoneIt != _lastZoneCallout.end() &&
        getMSTimeDiff(zoneIt->second, now) < sPlayerbotAIConfig.wpvpCalloutZoneCooldown * IN_MILLISECONDS)
        return false;

    auto attackerIt = _lastAttackerCallout.find(attacker);
    if (attackerIt != _lastAttackerCallout.end() &&
        getMSTimeDiff(attackerIt->second, now) < sPlayerbotAIConfig.wpvpCalloutAttackerCooldown * IN_MILLISECONDS)
        return false;

    return true;
}

void WpvpCalloutThrottle::Prune(uint32 now)
{
    // Both cooldowns are minutes-scale; anything older than an hour is dead
    // weight either way.
    constexpr uint32 staleAfter = HOUR * IN_MILLISECONDS;
    if (getMSTimeDiff(_lastPruneMs, now) < staleAfter / 4)
        return;

    _lastPruneMs = now;
    std::erase_if(_lastZoneCallout, [&](auto const& entry) { return getMSTimeDiff(entry.second, now) > staleAfter; });
    std::erase_if(_lastAttackerCallout,
                  [&](auto const& entry) { return getMSTimeDiff(entry.second, now) > staleAfter; });
}

Player* FindWpvpIntruder(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    if (Unit* attacker = bot->getAttackerForHelper())
        if (Player* enemy = attacker->ToPlayer())
            if (botAI->IsOpposing(enemy))
                return enemy;

    // "nearest enemy players" is IsPvP-gated, which is exactly right:
    // invaders are always flagged.
    GuidVector enemies = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (ObjectGuid const guid : enemies)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (Player* enemy = unit->ToPlayer())
            return enemy;
    }

    return nullptr;
}

bool WpvpDefenseCalloutTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.wpvpCalloutEnabled)
        return false;

    if (bot->InBattleground() || bot->InArena())
        return false;

    if (!sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    // Invaders don't report themselves to the defenders' channel.
    if (botAI->rpgInfo.GetStatus() == RPG_GO_WPVP)
        return false;

    Player* intruder = FindWpvpIntruder(botAI);
    if (!intruder)
        return false;

    return WpvpCalloutThrottle::instance().CanReport(bot->GetTeamId(), bot->GetZoneId(), intruder->GetGUID());
}

bool WpvpDefenseCalloutAction::Execute(Event /*event*/)
{
    Player* intruder = FindWpvpIntruder(botAI);
    if (!intruder)
        return false;

    // Atomic check-and-record: when several defenders spot the same invader
    // in the same tick, only the first one gets to shout.
    if (!WpvpCalloutThrottle::instance().TryReport(bot->GetTeamId(), bot->GetZoneId(), intruder->GetGUID()))
        return false;

    AreaTableEntry const* area = sAreaTableStore.LookupEntry(bot->GetAreaId());
    if (!area)
        area = sAreaTableStore.LookupEntry(bot->GetZoneId());
    std::string areaName = PlayerbotAI::GetLocalizedAreaName(area);
    std::string name = intruder->GetName();

    std::string msg;
    switch (urand(0, 3))
    {
        case 0:
            msg = Acore::StringFormat("{} is attacking {}!", name, areaName);
            break;
        case 1:
            msg = Acore::StringFormat("Enemy spotted: {} near {}.", name, areaName);
            break;
        case 2:
            msg = Acore::StringFormat("We're under attack at {}! It's {}.", areaName, name);
            break;
        default:
            msg = Acore::StringFormat("{} needs defenders - {} is here!", areaName, name);
            break;
    }

    // The callout puts the invader on the defense board (so responders have
    // somewhere to go) and always fires the speech notification; whether WE
    // say the prebaked line is a separate gate, so mod-llm can supply the
    // words instead.
    WpvpDefenseBoard::instance().PostCallout(intruder, bot->GetTeamId());

    WpvpCalloutNotification notification;
    notification.kind = WpvpCalloutKind::FirstCallout;
    notification.speaker = bot;
    notification.zoneId = bot->GetZoneId();
    notification.areaName = areaName;
    notification.attackerName = name;
    notification.attackerRace = intruder->getRace();
    notification.attackerClass = intruder->getClass();
    notification.attackerLevel = intruder->GetLevel();
    notification.prebakedLine = msg;
    FireWpvpCalloutNotification(notification);

    if (!sPlayerbotAIConfig.wpvpCallouts)
        return true;

    return botAI->SayToChannel(msg, ChatChannelId::LOCAL_DEFENSE);
}
