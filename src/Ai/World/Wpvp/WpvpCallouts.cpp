#include "WpvpCallouts.h"

#include "DBCStores.h"
#include "LevelPerception.h"
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

namespace
{
// Combat with the defending side is what makes an enemy report-worthy;
// fighting mobs is just leveling. Checks both directions - who the enemy is
// swinging at and who is swinging at them - since a caster kiting guards has
// no melee victim of its own.
bool ObservedHostility(Player* enemy, Player* bot, WpvpIntruderSighting& out)
{
    auto defenderSide = [&](Unit* unit) { return unit && unit->IsFriendlyTo(bot); };

    Unit* foe = defenderSide(enemy->GetVictim()) ? enemy->GetVictim() : nullptr;
    if (!foe)
        for (Unit* attacker : enemy->getAttackers())
            if (defenderSide(attacker))
            {
                foe = attacker;
                break;
            }

    if (!foe)
        return false;

    // A pet in the fight means its owner is in the fight.
    if (Player* victim = foe->GetCharmerOrOwnerPlayerOrPlayerItself())
    {
        out.activity = WpvpCalloutActivity::AttackingPlayer;
        out.victimName = victim->GetName();
    }
    else
        out.activity = WpvpCalloutActivity::AttackingNpcs;

    return true;
}
}

bool FindWpvpIntruder(PlayerbotAI* botAI, WpvpIntruderSighting& out)
{
    Player* bot = botAI->GetBot();

    // An enemy the bot outlevels by the gank gap is prey, not peril: real
    // players squash those, they don't announce them to the zone.
    auto alarmWorthy = [&](Player* enemy)
    { return bot->GetLevel() < PerceivedLevel(bot, enemy) + sPlayerbotAIConfig.wpvpGankLevelGap; };

    auto reportWorthy = [&](Player* enemy)
    {
        if (!alarmWorthy(enemy))
            return false;

        if (ObservedHostility(enemy, bot, out))
        {
            out.intruder = enemy;
            return true;
        }

        // Not seen fighting anyone of ours: only worth naming if the defense
        // channels already know this one - a fresh, called-out board entry.
        if (WpvpDefenseBoard::instance().IsKnownThreat(enemy->GetGUID(), bot->GetTeamId()))
        {
            out.intruder = enemy;
            out.activity = WpvpCalloutActivity::Prowling;
            out.victimName.clear();
            return true;
        }

        return false;
    };

    if (Unit* attacker = bot->getAttackerForHelper())
        if (Player* enemy = attacker->ToPlayer())
            if (botAI->IsOpposing(enemy) && reportWorthy(enemy))
                return true;

    // "nearest enemy players" is IsPvP-gated, which on a PvP-type realm means
    // everyone in a contested zone - it narrows the candidates, but only
    // observed hostility (or a known ganker) qualifies one.
    GuidVector enemies = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (ObjectGuid const guid : enemies)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (Player* enemy = unit->ToPlayer())
            if (reportWorthy(enemy))
                return true;
    }

    return false;
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

    WpvpIntruderSighting sighting;
    if (!FindWpvpIntruder(botAI, sighting))
        return false;

    return WpvpCalloutThrottle::instance().CanReport(bot->GetTeamId(), bot->GetZoneId(),
                                                     sighting.intruder->GetGUID());
}

bool WpvpDefenseCalloutAction::Execute(Event /*event*/)
{
    WpvpIntruderSighting sighting;
    if (!FindWpvpIntruder(botAI, sighting))
        return false;

    Player* intruder = sighting.intruder;

    // Atomic check-and-record: when several defenders spot the same invader
    // in the same tick, only the first one gets to shout.
    if (!WpvpCalloutThrottle::instance().TryReport(bot->GetTeamId(), bot->GetZoneId(), intruder->GetGUID()))
        return false;

    AreaTableEntry const* area = sAreaTableStore.LookupEntry(bot->GetAreaId());
    if (!area)
        area = sAreaTableStore.LookupEntry(bot->GetZoneId());
    std::string areaName = PlayerbotAI::GetLocalizedAreaName(area);
    std::string name = intruder->GetName();

    // The line has to match what was seen: "attacking <area>" is for someone
    // actually hitting the defenders' NPCs, not for whatever camp the
    // spotter happens to stand in.
    std::string msg;
    switch (sighting.activity)
    {
        case WpvpCalloutActivity::AttackingPlayer:
            if (sighting.victimName == bot->GetName())
                msg = urand(0, 1)
                    ? Acore::StringFormat("{} jumped me near {}! Little help?", name, areaName)
                    : Acore::StringFormat("Under attack at {} - it's {}!", areaName, name);
            else
                msg = urand(0, 1)
                    ? Acore::StringFormat("{} is attacking {} near {}!", name, sighting.victimName, areaName)
                    : Acore::StringFormat("{} needs help at {} - {} is on them!", sighting.victimName, areaName,
                                          name);
            break;
        case WpvpCalloutActivity::Prowling:
            msg = urand(0, 1)
                ? Acore::StringFormat("That ganker {} is prowling around {} now.", name, areaName)
                : Acore::StringFormat("Watch yourselves - {} was just spotted near {}.", name, areaName);
            break;
        default:
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
            break;
    }

    // The callout puts the invader on the defense board (so responders have
    // somewhere to go) and always fires the speech notification; whether WE
    // say the prebaked line is a separate gate, so mod-llm can supply the
    // words instead.
    WpvpDefenseBoard::instance().PostCallout(intruder, bot->GetTeamId(), bot);

    WpvpCalloutNotification notification;
    notification.kind = WpvpCalloutKind::FirstCallout;
    notification.speaker = bot;
    notification.zoneId = bot->GetZoneId();
    notification.areaName = areaName;
    notification.attackerName = name;
    notification.attackerRace = intruder->getRace();
    notification.attackerClass = intruder->getClass();
    notification.attackerLevelText = PerceivedLevelText(bot, intruder);
    notification.activity = sighting.activity;
    notification.victimName = sighting.victimName;
    notification.prebakedLine = msg;
    FireWpvpCalloutNotification(notification);

    if (!sPlayerbotAIConfig.wpvpCallouts)
        return true;

    return botAI->SayToChannel(msg, ChatChannelId::LOCAL_DEFENSE);
}
