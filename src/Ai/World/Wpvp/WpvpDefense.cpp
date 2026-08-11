#include "WpvpDefense.h"

#include <algorithm>
#include <cctype>

#include "BotDeathSafety.h"
#include "ChatHelper.h"
#include "CombatManager.h"
#include "DBCStores.h"
#include "FelworldEvents.h"
#include "LevelPerception.h"
#include "Metric.h"
#include "NewRpgInfo.h"
#include "NewRpgWpvp.h"
#include "ObjectAccessor.h"
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
// How long a board entry stays at all, and how long after its last update it
// still draws responders. Gankers move on; stale intel sends bots to empty
// fields.
constexpr uint32 ENTRY_TTL_MS = 30 * MINUTE * IN_MILLISECONDS;
constexpr uint32 RESPONDABLE_WINDOW_MS = 10 * MINUTE * IN_MILLISECONDS;

// How recently the defending side must have drawn blood in the attacker's
// zone for the fight to count as contested at the moment a spree would arm
// an escalation. Everything after arming is handled by event-driven
// cancellation; this window only papers over the gap where the defense's
// last kill predates the threshold-crossing one.
constexpr uint32 CONTESTED_WINDOW_MS = 2 * MINUTE * IN_MILLISECONDS;

// How long after the last kill or callout in a zone the fight still counts
// as "happening" for the leisure-suppression check. Short on purpose: real
// players do duel while waiting for enemies to come back, just not while
// bodies are still warm.
constexpr uint32 RECENT_FIGHT_WINDOW_MS = 2 * MINUTE * IN_MILLISECONDS;

uint64 RollKey(ObjectGuid bot, ObjectGuid attacker) { return bot.GetRawValue() ^ (attacker.GetRawValue() << 1); }

uint64 EscalationCooldownKey(TeamId team, uint32 zoneId) { return (uint64(team) << 32) | zoneId; }

// The fight is contested: the pending plea (if any) is off, and the shout
// must be re-earned with a whole new uncontested spree. Deliberately biased
// toward "this is handled" - a false silence costs three more corpses, a
// false alarm spams a faction-wide channel.
void CancelEscalation(WpvpDefenseEntry& entry)
{
    entry.escalationPending = false;
    entry.kills = 0;
    entry.firstKillMs = 0;
}

std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string Trimmed(std::string const& s)
{
    size_t begin = s.find_first_not_of(" \t");
    size_t end = s.find_last_not_of(" \t");
    return begin == std::string::npos ? "" : s.substr(begin, end - begin + 1);
}

char const* Article(std::string const& word)
{
    if (word.empty())
        return "a";

    switch (std::tolower(static_cast<unsigned char>(word[0])))
    {
        case 'a': case 'e': case 'i': case 'o': case 'u':
            return "an";
        default:
            return "a";
    }
}

std::vector<std::function<void(WpvpCalloutNotification const&)>>& Listeners()
{
    static std::vector<std::function<void(WpvpCalloutNotification const&)>> listeners;
    return listeners;
}

std::mutex& ListenersMutex()
{
    static std::mutex mutex;
    return mutex;
}

// `observer` is whoever the facts come from - the bot that spotted the ganker,
// the victim they just killed - so the level is filed as that player saw it.
// Without one (nobody left to ask) the level already on the board stands.
void RefreshAttackerFacts(WpvpDefenseEntry& entry, Player* attacker, Player const* observer, uint32 now)
{
    if (!entry.postedMs)
    {
        entry.attacker = attacker->GetGUID();
        entry.postedMs = now;
    }

    entry.zoneId = attacker->GetZoneId();
    entry.pos = WorldPosition(attacker);
    entry.attackerName = attacker->GetName();
    entry.attackerRace = attacker->getRace();
    entry.attackerClass = attacker->getClass();
    if (observer)
        entry.attackerLevel = PerceivedLevel(observer, attacker);
    entry.updatedMs = now;
}

// First-hand knowledge only: an escalation shout comes from a victim of the
// spree or a bot that has the ganker on its screen right now - never from a
// stranger relaying news it couldn't know. Victims count only while they're
// still in the ganker's zone: a live plea comes from where the trouble is,
// and a victim who released and moved on is out of the story.
bool IsEscalationEyewitness(Player* bot, WpvpDefenseEntry const& entry)
{
    // Only the genuinely outmatched plead in WorldDefense: anyone within the
    // gank gap of the ganker's level - victims included - is expected to
    // handle it, not broadcast it (defense responses recruit exactly these
    // bots, and arrivals parked in vision range would otherwise claim the
    // shout).
    if (bot->GetLevel() + sPlayerbotAIConfig.wpvpGankLevelGap > entry.attackerLevel)
        return false;

    if (bot->GetMapId() == entry.pos.GetMapId() && bot->GetZoneId() == entry.zoneId &&
        std::find(entry.victims.begin(), entry.victims.end(), bot->GetGUID()) != entry.victims.end())
        return true;

    Player* attacker = ObjectAccessor::FindPlayer(entry.attacker);
    return attacker && attacker->IsInWorld() && attacker->GetMapId() == bot->GetMapId() &&
           bot->IsWithinDist(attacker, sPlayerbotAIConfig.wpvpVisionDistance);
}

// Zone whose enUS name matches. Top-level zones win (exactly first, then
// substring, so "redridge" finds Redridge Mountains); subzone names resolve
// to their parent zone, so "tarren mill" finds Hillsbrad Foothills - people
// calling for help name the place they see, not the zone on the map.
uint32 ResolveZoneIdByName(std::string const& name)
{
    std::string want = ToLower(Trimmed(name));
    if (want.empty())
        return 0;

    uint32 partialZone = 0;
    uint32 exactSubzone = 0;
    uint32 partialSubzone = 0;
    for (uint32 i = 0; i < sAreaTableStore.GetNumRows(); ++i)
    {
        AreaTableEntry const* area = sAreaTableStore.LookupEntry(i);
        if (!area || !area->area_name[0])
            continue;

        std::string have = ToLower(area->area_name[0]);
        if (!area->zone)
        {
            if (have == want)
                return area->ID;

            if (!partialZone && have.find(want) != std::string::npos)
                partialZone = area->ID;
        }
        else
        {
            if (!exactSubzone && have == want)
                exactSubzone = area->zone;

            if (!partialSubzone && have.find(want) != std::string::npos)
                partialSubzone = area->zone;
        }
    }

    if (exactSubzone)
        return exactSubzone;

    return partialZone ? partialZone : partialSubzone;
}
}

void RegisterWpvpCalloutListener(std::function<void(WpvpCalloutNotification const&)> listener)
{
    std::lock_guard<std::mutex> lock(ListenersMutex());
    Listeners().push_back(std::move(listener));
}

void FireWpvpCalloutNotification(WpvpCalloutNotification const& notification)
{
    std::lock_guard<std::mutex> lock(ListenersMutex());
    for (auto const& listener : Listeners())
        listener(notification);
}

void WpvpDefenseBoard::PostCallout(Player* attacker, TeamId defendingTeam, Player const* spotter)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    WpvpDefenseEntry& entry = _entries[attacker->GetGUID()];
    RefreshAttackerFacts(entry, attacker, spotter, now);
    entry.defendingTeam = defendingTeam;
    entry.calledOutMs = now;

    METRIC_VALUE("playerbots_wpvp", 1, METRIC_TAG("event", "callout"));
    Felworld::LogEvent(attacker->GetGUID(), "wpvp_callout",
                       Acore::StringFormat("{{\"zone\":{}}}", entry.zoneId));

    Prune(now);
}

void WpvpDefenseBoard::RecordKill(Player* attacker, Player* victim)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    WpvpDefenseEntry& entry = _entries[attacker->GetGUID()];
    RefreshAttackerFacts(entry, attacker, victim, now);
    entry.defendingTeam = victim->GetTeamId();

    // Telemetry is bookkeeping, not bot knowledge: it logs the true levels.
    METRIC_VALUE("playerbots_wpvp", 1, METRIC_TAG("event", "kill"));
    Felworld::LogEvent(attacker->GetGUID(), "wpvp_kill",
                       Acore::StringFormat("{{\"victim\":\"{}\",\"victim_level\":{},\"zone\":{}}}",
                                           victim->GetName(), victim->GetLevel(), entry.zoneId));
    Felworld::LogEvent(victim->GetGUID(), "wpvp_death",
                       Acore::StringFormat("{{\"killer\":\"{}\",\"killer_level\":{},\"zone\":{}}}",
                                           attacker->GetName(), attacker->GetLevel(), entry.zoneId));

    // Joiner exemption (Felworld): a passerby who piled into an ongoing even
    // fight and died as the add doesn't feed the spree - everyone watching
    // saw a battle, not a gank. Evidence: the killer is still trading blows
    // with a living even-match opponent.
    bool addKill = false;
    for (auto const& [guid, combatRef] : attacker->GetCombatManager().GetPvPCombatRefs())
    {
        Unit* other = combatRef->GetOther(attacker);
        if (!other || other == victim || !other->IsPlayer() || !other->IsAlive())
            continue;

        if (uint32(other->GetLevel()) + sPlayerbotAIConfig.wpvpGankLevelGap > entry.attackerLevel)
        {
            addKill = true;
            break;
        }
    }

    // Only genuine gank kills - victim a full gank gap below the attacker -
    // feed the spree tally: no number of even fights lost fair and square
    // warrants a WorldDefense plea. The attacker's level is the one the
    // victim could see; a skull already means further above than the gap.
    if (!addKill && uint32(victim->GetLevel()) + sPlayerbotAIConfig.wpvpGankLevelGap <= entry.attackerLevel)
    {
        if (!entry.firstKillMs ||
            getMSTimeDiff(entry.firstKillMs, now) > sPlayerbotAIConfig.wpvpEscalationWindow * IN_MILLISECONDS)
        {
            entry.firstKillMs = now;
            entry.kills = 1;
        }
        else
            ++entry.kills;

        if (!entry.escalated && entry.kills >= sPlayerbotAIConfig.wpvpEscalationKills &&
            !CapableDefenseActive(entry, now))
            entry.escalationPending = true;
    }

    // Remember who this ganker got - even across tally resets: outmatched
    // victims are the natural escalation shouters ("keeps killing me!"),
    // and deaths to non-victims are what arm reinforcements.
    if (std::find(entry.victims.begin(), entry.victims.end(), victim->GetGUID()) == entry.victims.end())
    {
        entry.victims.push_back(victim->GetGUID());
        if (entry.victims.size() > 16)
            entry.victims.erase(entry.victims.begin());
    }

    // Who this attacker preys on decides how urgent defense feels: lowbie
    // ganking pulls the full response chance, an even brawl much less.
    entry.maxVictimLevel = std::max(entry.maxVictimLevel, static_cast<uint8>(victim->GetLevel()));

    // The same kill is proof the killer's side has a capable fighter on this
    // battlefield: every tracked ganker of the OTHER side in the zone whom
    // the killer is a level match for is now contested. In a pitched battle
    // both sides score constantly, so both sides' alarms stay silent.
    for (auto& [guid, other] : _entries)
    {
        if (other.defendingTeam != attacker->GetTeamId() || other.zoneId != entry.zoneId)
            continue;

        if (uint32(entry.attackerLevel) + sPlayerbotAIConfig.wpvpGankLevelGap <= other.attackerLevel)
            continue;

        CancelEscalation(other);
    }

    Prune(now);
}

void WpvpDefenseBoard::RecordZoneUnderAttack(Player* attacker, TeamId defendingTeam, uint8 npcLevel)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    WpvpDefenseEntry& entry = _entries[attacker->GetGUID()];
    RefreshAttackerFacts(entry, attacker, nullptr, now);
    entry.defendingTeam = defendingTeam;

    // Only fill in the level when nobody has actually seen the attacker: the
    // guard-kill inference is a floor, not a sighting, and must never
    // overwrite a real eyewitness read.
    if (!entry.attackerLevel)
        entry.attackerLevel = npcLevel;

    // The server's faction-wide broadcast is the callout.
    entry.calledOutMs = now;

    METRIC_VALUE("playerbots_wpvp", 1, METRIC_TAG("event", "zone_under_attack"));
    Felworld::LogEvent(attacker->GetGUID(), "wpvp_zone_under_attack",
                       Acore::StringFormat("{{\"zone\":{},\"npc_level\":{}}}", entry.zoneId, npcLevel));

    Prune(now);
}

void WpvpDefenseBoard::RecordAttackerDeath(Player* attacker, ObjectGuid killer)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(attacker->GetGUID());
    if (it == _entries.end())
        return;

    WpvpDefenseEntry& entry = it->second;

    Player* killerPlayer = ObjectAccessor::FindPlayer(killer);

    // The spree is contested: the tally starts over, and a shout that hasn't
    // been claimed yet would already be stale news. The death spot is the
    // freshest intel on where the fight is, and whoever put them down is who
    // saw them up close.
    RefreshAttackerFacts(entry, attacker, killerPlayer, getMSTime());
    entry.kills = 0;
    entry.firstKillMs = 0;
    entry.escalationPending = false;

    METRIC_VALUE("playerbots_wpvp", 1, METRIC_TAG("event", "attacker_death"));
    std::string killerName;
    if (killerPlayer)
        killerName = killerPlayer->GetName();
    Felworld::LogEvent(attacker->GetGUID(), "wpvp_defeated",
                       Acore::StringFormat("{{\"killer\":\"{}\",\"zone\":{}}}", killerName, entry.zoneId));

    // A victim getting their own revenge settles the score quietly; dying
    // repeatedly to OUTSIDE help - defenders who were never on the menu -
    // is when the ganker backchannels their friends. One wave, ever.
    if (killer && std::find(entry.victims.begin(), entry.victims.end(), killer) == entry.victims.end())
    {
        ++entry.avengedDeaths;
        if (!entry.reinforceArmedMs && entry.avengedDeaths >= sPlayerbotAIConfig.wpvpReinforcementDeaths)
            entry.reinforceArmedMs = getMSTime();
    }
}

void WpvpDefenseBoard::NoteDefenderOnScene(ObjectGuid attacker, TeamId team, uint8 defenderLevel)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(attacker);
    if (it == _entries.end())
        return;

    WpvpDefenseEntry& entry = it->second;
    if (entry.defendingTeam != team)
        return;

    METRIC_VALUE("playerbots_wpvp", 1, METRIC_TAG("event", "defender_on_scene"));

    // Anyone within the gank gap of the ganker is a handler, not a shouter
    // (the same line the eyewitness rule draws): their arrival means the
    // fight is handled, and a defender later dying is a normal part of
    // defending - not by itself grounds for a fresh faction-wide plea.
    if (uint32(defenderLevel) + sPlayerbotAIConfig.wpvpGankLevelGap <= entry.attackerLevel)
        return;

    CancelEscalation(entry);
}

// A fighter from the defending side has drawn blood in the entry's zone
// recently: their own attacker entry on the mirror side of the board is the
// evidence (the board can't see the world, only kills and callouts).
bool WpvpDefenseBoard::CapableDefenseActive(WpvpDefenseEntry const& entry, uint32 now) const
{
    for (auto const& [guid, other] : _entries)
    {
        if (other.defendingTeam == entry.defendingTeam || other.defendingTeam == TEAM_NEUTRAL)
            continue;

        if (other.zoneId != entry.zoneId)
            continue;

        if (getMSTimeDiff(other.updatedMs, now) >= CONTESTED_WINDOW_MS)
            continue;

        if (uint32(other.attackerLevel) + sPlayerbotAIConfig.wpvpGankLevelGap > entry.attackerLevel)
            return true;
    }

    return false;
}

bool WpvpDefenseBoard::EscalationCoolingDown(TeamId team, uint32 zoneId, uint32 now) const
{
    auto it = _escalationShoutMs.find(EscalationCooldownKey(team, zoneId));
    return it != _escalationShoutMs.end() &&
           getMSTimeDiff(it->second, now) < sPlayerbotAIConfig.wpvpEscalationWindow * IN_MILLISECONDS;
}

std::vector<WpvpDefenseEntry> WpvpDefenseBoard::PendingEscalations(TeamId team)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    std::vector<WpvpDefenseEntry> pending;
    for (auto& [guid, entry] : _entries)
    {
        if (!entry.escalationPending || entry.defendingTeam != team)
            continue;

        // One shout per battlefield: while the faction's last WorldDefense
        // call about this zone is fresh, further pleas are redundant -
        // cancelled outright, not queued for later.
        if (EscalationCoolingDown(team, entry.zoneId, now))
        {
            CancelEscalation(entry);
            continue;
        }

        pending.push_back(entry);
    }

    return pending;
}

bool WpvpDefenseBoard::ClaimEscalation(TeamId team, ObjectGuid attacker, WpvpDefenseEntry& out)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(attacker);
    if (it == _entries.end())
        return false;

    WpvpDefenseEntry& entry = it->second;
    uint32 now = getMSTime();
    if (!entry.escalationPending || entry.defendingTeam != team)
        return false;

    // Two eyewitnesses racing for different gankers in the same zone: the
    // loser's plea dies here rather than becoming a second shout.
    if (EscalationCoolingDown(team, entry.zoneId, now))
    {
        CancelEscalation(entry);
        return false;
    }

    entry.escalationPending = false;
    entry.escalated = true;
    // A WorldDefense shout reaches the whole faction: it counts as the
    // callout that makes the entry respondable.
    entry.calledOutMs = now;
    entry.updatedMs = now;
    _escalationShoutMs[EscalationCooldownKey(team, entry.zoneId)] = now;
    out = entry;

    METRIC_VALUE("playerbots_wpvp", 1, METRIC_TAG("event", "escalation"));
    return true;
}

bool WpvpDefenseBoard::IsRespondable(WpvpDefenseEntry const& entry, uint32 now) const
{
    return entry.calledOutMs && getMSTimeDiff(entry.updatedMs, now) < RESPONDABLE_WINDOW_MS;
}

bool WpvpDefenseBoard::FindRespondable(TeamId team, uint8 botLevel, ObjectGuid botGuid, WpvpDefenseEntry& out)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    WpvpDefenseEntry const* best = nullptr;
    for (auto const& [guid, entry] : _entries)
    {
        if (entry.defendingTeam != team || !IsRespondable(entry, now))
            continue;

        if (sPlayerbotAIConfig.wpvpDefenseResponderCap &&
            entry.defenseResponses >= sPlayerbotAIConfig.wpvpDefenseResponderCap)
            continue;

        if (botLevel + sPlayerbotAIConfig.wpvpDefenseLevelSlack < entry.attackerLevel)
            continue;

        if (_responseRolls.count(RollKey(botGuid, entry.attacker)))
            continue;

        if (!best || entry.updatedMs > best->updatedMs)
            best = &entry;
    }

    if (!best)
        return false;

    out = *best;
    return true;
}

bool WpvpDefenseBoard::IsKnownThreat(ObjectGuid attacker, TeamId team)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(attacker);
    return it != _entries.end() && it->second.defendingTeam == team && IsRespondable(it->second, getMSTime());
}

bool WpvpDefenseBoard::FindByZone(TeamId team, uint32 zoneId, WpvpDefenseEntry& out)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    WpvpDefenseEntry const* best = nullptr;
    for (auto const& [guid, entry] : _entries)
    {
        // An explicit order doesn't need a prior callout - the requester saw
        // the trouble themselves - just intel that isn't stale.
        if (entry.defendingTeam != team || entry.zoneId != zoneId)
            continue;

        if (getMSTimeDiff(entry.updatedMs, now) >= RESPONDABLE_WINDOW_MS)
            continue;

        if (!best || entry.updatedMs > best->updatedMs)
            best = &entry;
    }

    if (!best)
        return false;

    out = *best;
    return true;
}

bool WpvpDefenseBoard::RecentActivityInZone(uint32 mapId, uint32 zoneId, uint32 windowMs)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    for (auto const& [guid, entry] : _entries)
        if (entry.pos.GetMapId() == mapId && entry.zoneId == zoneId && getMSTimeDiff(entry.updatedMs, now) < windowMs)
            return true;

    return false;
}

bool WpvpDefenseBoard::FindReinforceable(TeamId team, uint8 botLevel, ObjectGuid botGuid, WpvpDefenseEntry& out)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    WpvpDefenseEntry const* best = nullptr;
    for (auto const& [guid, entry] : _entries)
    {
        // The attacker's team is the one facing the defenders - i.e. NOT the
        // entry's defending team.
        if (entry.defendingTeam == team || entry.defendingTeam == TEAM_NEUTRAL)
            continue;

        if (!entry.reinforceArmedMs || getMSTimeDiff(entry.reinforceArmedMs, now) >= RESPONDABLE_WINDOW_MS)
            continue;

        if (sPlayerbotAIConfig.wpvpReinforcementCap &&
            entry.reinforceResponses >= sPlayerbotAIConfig.wpvpReinforcementCap)
            continue;

        if (botLevel + sPlayerbotAIConfig.wpvpDefenseLevelSlack < entry.attackerLevel)
            continue;

        if (_responseRolls.count(RollKey(botGuid, entry.attacker)))
            continue;

        if (!best || entry.updatedMs > best->updatedMs)
            best = &entry;
    }

    if (!best)
        return false;

    out = *best;
    return true;
}

bool WpvpDefenseBoard::TryClaimResponseRoll(ObjectGuid bot, ObjectGuid attacker)
{
    std::lock_guard<std::mutex> lock(_mutex);

    // Cheap unbounded-growth backstop; entries rebuild harmlessly.
    if (_responseRolls.size() > 8192)
        _responseRolls.clear();

    return _responseRolls.insert(RollKey(bot, attacker)).second;
}

bool WpvpDefenseBoard::TryClaimResponseSlot(ObjectGuid attacker, bool reinforce)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(attacker);
    if (it == _entries.end())
        return false;

    WpvpDefenseEntry& entry = it->second;
    uint32 cap = reinforce ? sPlayerbotAIConfig.wpvpReinforcementCap : sPlayerbotAIConfig.wpvpDefenseResponderCap;
    uint32& taken = reinforce ? entry.reinforceResponses : entry.defenseResponses;
    if (cap && taken >= cap)
        return false;

    ++taken;
    return true;
}

void WpvpDefenseBoard::Prune(uint32 now)
{
    if (getMSTimeDiff(_lastPruneMs, now) < 5 * MINUTE * IN_MILLISECONDS)
        return;

    _lastPruneMs = now;
    std::erase_if(_entries, [&](auto const& pair) { return getMSTimeDiff(pair.second.updatedMs, now) > ENTRY_TTL_MS; });
    std::erase_if(_escalationShoutMs, [&](auto const& pair)
                  { return getMSTimeDiff(pair.second, now) >= sPlayerbotAIConfig.wpvpEscalationWindow * IN_MILLISECONDS; });
}

bool WpvpHappeningNearby(Player* bot)
{
    if (WpvpDefenseBoard::instance().RecentActivityInZone(bot->GetMapId(), bot->GetZoneId(), RECENT_FIGHT_WINDOW_MS))
        return true;

    // The board only hears about kills and callouts; a fight still in its
    // opening seconds - or a flagged ganker prowling between victims - shows
    // up here instead.
    return BotDeathSafety::EnemyPlayerNear(bot, sPlayerbotAIConfig.wpvpVisionDistance);
}

bool StartWpvpDefenseResponse(PlayerbotAI* botAI, uint32 zoneId, WorldPosition const& target, ObjectGuid attacker)
{
    WorldLocation loc(target.GetMapId(), target.GetPositionX(), target.GetPositionY(), target.GetPositionZ(),
                      target.GetOrientation());
    NewRpgInfo::GoWpvp payload;
    if (!ComputeWpvpPositions(loc, zoneId, payload))
        return false;

    payload.defend = true;
    payload.defendTarget = attacker;

    Player* bot = botAI->GetBot();
    if (bot->GetMapId() == target.GetMapId() && bot->GetZoneId() == zoneId)
    {
        // Already on the scene: no teleport, just run over.
        payload.teleported = true;
    }
    else
    {
        // The cavalry doesn't arrive instantly: hold departure for a rough
        // ride-there estimate, scaled by the tunable delay factor.
        float seconds = bot->GetMapId() == target.GetMapId()
                            ? WorldPosition(bot).distance(target) / 15.0f  // ground-mount pace
                            : 240.0f;                                      // boat/zeppelin hop plus the ride
        seconds = std::clamp(seconds * sPlayerbotAIConfig.wpvpDefenseDelayFactor, 10.0f, 240.0f);
        payload.departT = getMSTime() + uint32(seconds) * IN_MILLISECONDS;
    }

    LOG_DEBUG("playerbots", "[New RPG] Bot {} responds to defense callout in zone {} (attacker {}, departs in {}s)",
              bot->GetName(), zoneId, attacker.ToString(),
              payload.departT ? (payload.departT - getMSTime()) / IN_MILLISECONDS : 0);
    botAI->rpgInfo.ChangeToGoWpvp(std::move(payload));

    METRIC_VALUE("playerbots_wpvp_excursion_start", 1, METRIC_TAG("origin", "defense"));
    Felworld::LogEvent(bot->GetGUID(), "wpvp_excursion_start",
                       Acore::StringFormat("{{\"origin\":\"defense\",\"zone\":{}}}", zoneId));
    return true;
}

bool WpvpEscalationCalloutTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.wpvpCalloutEnabled)
        return false;

    if (bot->InBattleground() || bot->InArena())
        return false;

    if (!sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    for (WpvpDefenseEntry const& entry : WpvpDefenseBoard::instance().PendingEscalations(bot->GetTeamId()))
        if (IsEscalationEyewitness(bot, entry))
            return true;

    return false;
}

bool WpvpEscalationCalloutAction::Execute(Event /*event*/)
{
    WpvpDefenseEntry entry;
    bool claimed = false;
    for (WpvpDefenseEntry const& pending : WpvpDefenseBoard::instance().PendingEscalations(bot->GetTeamId()))
    {
        if (!IsEscalationEyewitness(bot, pending))
            continue;

        if (WpvpDefenseBoard::instance().ClaimEscalation(bot->GetTeamId(), pending.attacker, entry))
        {
            claimed = true;
            break;
        }
    }

    if (!claimed)
        return false;

    AreaTableEntry const* area = sAreaTableStore.LookupEntry(entry.zoneId);
    std::string areaName = PlayerbotAI::GetLocalizedAreaName(area);
    std::string race = ToLower(ChatHelper::FormatRace(entry.attackerRace));
    std::string cls = ToLower(ChatHelper::FormatClass(entry.attackerClass));

    std::string msg;
    switch (urand(0, 3))
    {
        case 0:
            msg = Acore::StringFormat("Getting griefed by {} {} {} in {} - {} dead already.", Article(race), race, cls,
                                      areaName, entry.kills);
            break;
        case 1:
            msg = Acore::StringFormat("There's {} {} {} picking people off in {}. Anyone free to help?", Article(race),
                                      race, cls, areaName);
            break;
        case 2:
            msg = Acore::StringFormat("{}, {} {}, is killing everyone in {}. Send help.", entry.attackerName, race, cls,
                                      areaName);
            break;
        default:
            msg = Acore::StringFormat("We've got {} {} {} camping {}. Could use a hand.", Article(race), race, cls,
                                      areaName);
            break;
    }

    WpvpCalloutNotification notification;
    notification.kind = WpvpCalloutKind::Escalation;
    notification.speaker = bot;
    notification.zoneId = entry.zoneId;
    notification.areaName = areaName;
    notification.attackerName = entry.attackerName;
    notification.attackerRace = entry.attackerRace;
    notification.attackerClass = entry.attackerClass;
    // Even relaying the board, the shouter can't quote a number past their
    // own skull threshold - "??" is all they'd have seen themselves.
    notification.attackerLevelText =
        IsLevelKnown(bot->GetLevel(), entry.attackerLevel, true) ? std::to_string(entry.attackerLevel) : "??";
    notification.killCount = entry.kills;
    notification.prebakedLine = msg;
    FireWpvpCalloutNotification(notification);

    if (!sPlayerbotAIConfig.wpvpCallouts)
        return true;

    return botAI->SayToChannel(msg, ChatChannelId::WORLD_DEFENSE);
}

bool WpvpDefenseResponseTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.wpvpDefenseEnabled)
        return false;

    if (bot->InBattleground() || bot->InArena() || bot->GetGroup() || bot->IsInCombat())
        return false;

    if (!sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    if (bot->GetLevel() < sPlayerbotAIConfig.wpvpMinBotLevel)
        return false;

    // Only bots that are genuinely between things drop what they're doing.
    switch (botAI->rpgInfo.GetStatus())
    {
        case RPG_IDLE:
        case RPG_REST:
        case RPG_WANDER_RANDOM:
        case RPG_WANDER_NPC:
        case RPG_GO_GRIND:
        case RPG_GO_CAMP:
            break;
        default:
            return false;
    }

    WpvpDefenseEntry entry;
    return WpvpDefenseBoard::instance().FindRespondable(bot->GetTeamId(), bot->GetLevel(), bot->GetGUID(), entry);
}

bool WpvpDefenseResponseAction::Execute(Event /*event*/)
{
    WpvpDefenseEntry entry;
    if (!WpvpDefenseBoard::instance().FindRespondable(bot->GetTeamId(), bot->GetLevel(), bot->GetGUID(), entry))
        return false;

    // One roll per bot per ganker, pass or fail - so the same callout doesn't
    // grind away at every idle bot until everyone eventually shows up.
    if (!WpvpDefenseBoard::instance().TryClaimResponseRoll(bot->GetGUID(), entry.attacker))
        return false;

    // Everyone drops what they're doing to stop a lowbie ganker; an evenly
    // matched brawl (which is what reinforcement fights look like from the
    // other side) draws far fewer volunteers. No victims yet means a fresh
    // sighting callout - treat it as worth answering.
    float chance = sPlayerbotAIConfig.wpvpDefenseResponseChance;
    if (entry.maxVictimLevel &&
        entry.attackerLevel < uint32(entry.maxVictimLevel) + sPlayerbotAIConfig.wpvpGankLevelGap)
        chance = sPlayerbotAIConfig.wpvpDefenseEvenFightChance;

    if (frand(0.0f, 100.0f) >= chance)
        return false;

    if (!WpvpDefenseBoard::instance().TryClaimResponseSlot(entry.attacker, false))
        return false;

    return StartWpvpDefenseResponse(botAI, entry.zoneId, entry.pos, entry.attacker);
}

bool WpvpReinforceTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.wpvpReinforcementEnabled)
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
        default:
            return false;
    }

    WpvpDefenseEntry entry;
    return WpvpDefenseBoard::instance().FindReinforceable(bot->GetTeamId(), bot->GetLevel(), bot->GetGUID(), entry);
}

bool WpvpReinforceAction::Execute(Event /*event*/)
{
    WpvpDefenseEntry entry;
    if (!WpvpDefenseBoard::instance().FindReinforceable(bot->GetTeamId(), bot->GetLevel(), bot->GetGUID(), entry))
        return false;

    if (!WpvpDefenseBoard::instance().TryClaimResponseRoll(bot->GetGUID(), entry.attacker))
        return false;

    if (frand(0.0f, 100.0f) >= sPlayerbotAIConfig.wpvpReinforcementChance)
        return false;

    if (!WpvpDefenseBoard::instance().TryClaimResponseSlot(entry.attacker, true))
        return false;

    // Same travel machinery as a defense response, but the "defend target"
    // is our own faction-mate: stick around while they're still in the
    // fight, drift home once they're gone for good.
    return StartWpvpDefenseResponse(botAI, entry.zoneId, entry.pos, entry.attacker);
}

bool WpvpDefendCommandAction::Execute(Event event)
{
    // Only random bots run the New-RPG machinery the defense response rides
    // on; a master-owned alt would just whisper a promise it can't keep.
    if (!sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    Player* requester = event.getOwner();
    std::string param = event.getParam();

    uint32 zoneId = 0;
    if (!Trimmed(param).empty())
        zoneId = ResolveZoneIdByName(param);
    else if (requester)
        zoneId = requester->GetZoneId();

    if (!zoneId)
    {
        if (requester && sPlayerbotAIConfig.wpvpCallouts)
            bot->Whisper(Acore::StringFormat("I don't know a zone called \"{}\".", Trimmed(param)), LANG_UNIVERSAL,
                         requester);
        return false;
    }

    TeamId team = bot->GetTeamId();
    WorldPosition target;
    ObjectGuid attacker;
    WpvpDefenseEntry entry;
    if (WpvpDefenseBoard::instance().FindByZone(team, zoneId, entry))
    {
        target = entry.pos;
        attacker = entry.attacker;
    }
    else if (requester && requester->IsInWorld() && !requester->GetMap()->Instanceable() &&
             requester->GetZoneId() == zoneId)
    {
        // No board entry - a player's chat report never files one - but the
        // caller is standing where the trouble is: make the stand at their
        // side.
        target = WorldPosition(requester);
    }
    else
    {
        // No live report from there: anchor on the zone's own travel hub.
        bool found = false;
        for (TravelMgr::WpvpHubInfo const& hub : sTravelMgr.GetWpvpHubs(team))
        {
            if (hub.zoneId != zoneId)
                continue;

            target = WorldPosition(hub.loc.GetMapId(), hub.loc.GetPositionX(), hub.loc.GetPositionY(),
                                   hub.loc.GetPositionZ(), hub.loc.GetOrientation());
            found = true;
            break;
        }

        if (!found)
        {
            if (requester && sPlayerbotAIConfig.wpvpCallouts)
                bot->Whisper("I wouldn't know where to make a stand there.", LANG_UNIVERSAL, requester);
            return false;
        }
    }

    if (!StartWpvpDefenseResponse(botAI, zoneId, target, attacker))
        return false;

    if (requester && sPlayerbotAIConfig.wpvpCallouts)
    {
        AreaTableEntry const* area = sAreaTableStore.LookupEntry(zoneId);
        bot->Whisper(Acore::StringFormat("On my way to {}.", PlayerbotAI::GetLocalizedAreaName(area)), LANG_UNIVERSAL,
                     requester);
    }

    return true;
}
