#include "WpvpDefense.h"

#include <algorithm>
#include <cctype>

#include "ChatHelper.h"
#include "DBCStores.h"
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

// How long after an outclassing defender was last seen with the attacker the
// escalation shout stays held. The dwell loop re-stamps every AI update, so
// this only needs to outlive that cadence; when the defender dies or leaves,
// the plea for help becomes honest again about this fast.
constexpr uint32 DEFENDER_ON_SCENE_WINDOW_MS = 30 * IN_MILLISECONDS;

uint64 RollKey(ObjectGuid bot, ObjectGuid attacker) { return bot.GetRawValue() ^ (attacker.GetRawValue() << 1); }

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

void RefreshAttackerFacts(WpvpDefenseEntry& entry, Player* attacker, uint32 now)
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
    entry.attackerLevel = attacker->GetLevel();
    entry.updatedMs = now;
}

// First-hand knowledge only: an escalation shout comes from a victim of the
// spree or a bot that has the ganker on its screen right now - never from a
// stranger across the zone relaying news it couldn't know. (A victim may
// shout from anywhere: being corpse-camped follows you home.)
bool IsEscalationEyewitness(Player* bot, WpvpDefenseEntry const& entry)
{
    if (std::find(entry.victims.begin(), entry.victims.end(), bot->GetGUID()) != entry.victims.end())
        return true;

    // A bystander who outlevels the ganker by the gank gap doesn't plead for
    // help - they ARE the help (defense responses recruit exactly these
    // bots, and arrivals parked in vision range would otherwise claim the
    // shout). Victims are exempt above: their grievance is first-hand at
    // any level.
    if (bot->GetLevel() >= entry.attackerLevel + sPlayerbotAIConfig.wpvpGankLevelGap)
        return false;

    Player* attacker = ObjectAccessor::FindPlayer(entry.attacker);
    return attacker && attacker->IsInWorld() && attacker->GetMapId() == bot->GetMapId() &&
           bot->IsWithinDist(attacker, sPlayerbotAIConfig.wpvpVisionDistance);
}

// Top-level zone whose enUS name matches (exactly first, then substring, so
// "redridge" finds Redridge Mountains).
uint32 ResolveZoneIdByName(std::string const& name)
{
    std::string want = ToLower(Trimmed(name));
    if (want.empty())
        return 0;

    uint32 partialMatch = 0;
    for (uint32 i = 0; i < sAreaTableStore.GetNumRows(); ++i)
    {
        AreaTableEntry const* area = sAreaTableStore.LookupEntry(i);
        if (!area || area->zone || !area->area_name[0])
            continue;

        std::string have = ToLower(area->area_name[0]);
        if (have == want)
            return area->ID;

        if (!partialMatch && have.find(want) != std::string::npos)
            partialMatch = area->ID;
    }

    return partialMatch;
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

void WpvpDefenseBoard::PostCallout(Player* attacker, TeamId defendingTeam)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    WpvpDefenseEntry& entry = _entries[attacker->GetGUID()];
    RefreshAttackerFacts(entry, attacker, now);
    entry.defendingTeam = defendingTeam;
    entry.calledOutMs = now;
    Prune(now);
}

void WpvpDefenseBoard::RecordKill(Player* attacker, Player* victim)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    WpvpDefenseEntry& entry = _entries[attacker->GetGUID()];
    RefreshAttackerFacts(entry, attacker, now);
    entry.defendingTeam = victim->GetTeamId();

    if (!entry.firstKillMs ||
        getMSTimeDiff(entry.firstKillMs, now) > sPlayerbotAIConfig.wpvpEscalationWindow * IN_MILLISECONDS)
    {
        entry.firstKillMs = now;
        entry.kills = 1;
    }
    else
        ++entry.kills;

    // Victims are the natural escalation shouters ("keeps killing me!"), so
    // remember who this ganker got - even across tally resets.
    if (std::find(entry.victims.begin(), entry.victims.end(), victim->GetGUID()) == entry.victims.end())
    {
        entry.victims.push_back(victim->GetGUID());
        if (entry.victims.size() > 16)
            entry.victims.erase(entry.victims.begin());
    }

    // Who this attacker preys on decides how urgent defense feels: lowbie
    // ganking pulls the full response chance, an even brawl much less.
    entry.maxVictimLevel = std::max(entry.maxVictimLevel, static_cast<uint8>(victim->GetLevel()));

    if (!entry.escalated && entry.kills >= sPlayerbotAIConfig.wpvpEscalationKills)
        entry.escalationPending = true;

    Prune(now);
}

void WpvpDefenseBoard::RecordAttackerDeath(Player* attacker, ObjectGuid killer)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(attacker->GetGUID());
    if (it == _entries.end())
        return;

    WpvpDefenseEntry& entry = it->second;

    // The spree is contested: the tally starts over, and a shout that hasn't
    // been claimed yet would already be stale news. The death spot is the
    // freshest intel on where the fight is.
    RefreshAttackerFacts(entry, attacker, getMSTime());
    entry.kills = 0;
    entry.firstKillMs = 0;
    entry.escalationPending = false;

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

    // Only an outclassing defender holds the shout: an evenly matched
    // arrival is joining a fight, not ending one, and more help is still a
    // reasonable ask.
    if (defenderLevel < entry.attackerLevel + sPlayerbotAIConfig.wpvpGankLevelGap)
        return;

    entry.defenderOnSceneMs = getMSTime();
}

bool WpvpDefenseBoard::HelpOnScene(WpvpDefenseEntry const& entry, uint32 now) const
{
    return entry.defenderOnSceneMs && getMSTimeDiff(entry.defenderOnSceneMs, now) < DEFENDER_ON_SCENE_WINDOW_MS;
}

std::vector<WpvpDefenseEntry> WpvpDefenseBoard::PendingEscalations(TeamId team)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint32 now = getMSTime();
    std::vector<WpvpDefenseEntry> pending;
    for (auto const& [guid, entry] : _entries)
        if (entry.escalationPending && entry.defendingTeam == team && !HelpOnScene(entry, now))
            pending.push_back(entry);

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
    if (!entry.escalationPending || entry.defendingTeam != team || HelpOnScene(entry, now))
        return false;

    entry.escalationPending = false;
    entry.escalated = true;
    // A WorldDefense shout reaches the whole faction: it counts as the
    // callout that makes the entry respondable.
    entry.calledOutMs = now;
    entry.updatedMs = now;
    out = entry;
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

void WpvpDefenseBoard::Prune(uint32 now)
{
    if (getMSTimeDiff(_lastPruneMs, now) < 5 * MINUTE * IN_MILLISECONDS)
        return;

    _lastPruneMs = now;
    std::erase_if(_entries, [&](auto const& pair) { return getMSTimeDiff(pair.second.updatedMs, now) > ENTRY_TTL_MS; });
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
    notification.attackerLevel = entry.attackerLevel;
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
