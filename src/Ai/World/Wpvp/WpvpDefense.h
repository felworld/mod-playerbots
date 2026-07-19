#ifndef PLAYERBOTS_WPVPDEFENSE_H
#define PLAYERBOTS_WPVPDEFENSE_H

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "Action.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "TravelMgr.h"
#include "Trigger.h"

class Player;
class PlayerbotAI;

// Defense speech events. The kind determines the channel the line belongs
// in: FirstCallout speaks in the zone's LocalDefense, Escalation in
// WorldDefense.
enum class WpvpCalloutKind : uint8
{
    FirstCallout,
    Escalation,
};

struct WpvpCalloutNotification
{
    WpvpCalloutKind kind;
    Player* speaker;
    uint32 zoneId{0};
    std::string areaName;      // localized, e.g. "Redridge Mountains"
    std::string attackerName;
    uint8 attackerRace{0};
    uint8 attackerClass{0};
    uint8 attackerLevel{0};
    uint32 killCount{0};       // Escalation only: uncontested kills so far
    std::string prebakedLine;  // the line playerbots itself would say
};

// Defense speech events are ALWAYS fired here, whether or not anyone is
// listening; whether playerbots itself says the prebaked line is a separate
// config gate (AiPlayerbot.WpvpCallouts). Another module (mod-llm) can
// register a listener and speak its own version instead. Register during
// script loading only; notifications fire from map-update threads, so
// listeners must be fast and must not touch the world state - copy the
// facts out and queue any real work.
void RegisterWpvpCalloutListener(std::function<void(WpvpCalloutNotification const&)> listener);
void FireWpvpCalloutNotification(WpvpCalloutNotification const& notification);

// One tracked ganker: where they were last reported, and how their
// uncontested-kill spree is going.
struct WpvpDefenseEntry
{
    ObjectGuid attacker;
    TeamId defendingTeam{TEAM_NEUTRAL};
    uint32 zoneId{0};
    WorldPosition pos;
    std::string attackerName;
    uint8 attackerRace{0};
    uint8 attackerClass{0};
    uint8 attackerLevel{0};
    uint32 postedMs{0};
    uint32 updatedMs{0};
    uint32 calledOutMs{0};        // 0 until some channel callout mentioned this attacker
    uint32 kills{0};              // uncontested kills in the current window
    uint32 firstKillMs{0};        // start of the current kill window
    bool escalationPending{false};
    bool escalated{false};        // the one WorldDefense shout has been made
};

// Shared bulletin board of active gankers, keyed by attacker. Producers:
// the LocalDefense callout action, the PVP-kill hook, escalation shouts.
// Consumers: the dice-path defense responders and the "wpvp defend"
// command. All state is mutex-guarded - producers and consumers run on
// different map-update threads.
class WpvpDefenseBoard
{
public:
    static WpvpDefenseBoard& instance()
    {
        static WpvpDefenseBoard instance;
        return instance;
    }

    // Upsert from a defense callout: refresh position/level facts and stamp
    // the entry as called out (responders only react to attackers somebody
    // actually mentioned in a channel - nobody is psychic).
    void PostCallout(Player* attacker, TeamId defendingTeam);

    // From the PVP-kill hook: bump the attacker's uncontested-kill tally,
    // arming a WorldDefense escalation at the configured threshold.
    void RecordKill(Player* attacker, Player* victim);

    // A tracked ganker died: the spree is contested, the tally resets and
    // any not-yet-claimed escalation is cancelled.
    void RecordAttackerDeath(ObjectGuid attacker);

    // Escalation is claimed by a bot in the ganker's zone (it plausibly saw
    // the LocalDefense traffic). Claim is atomic: exactly one bot shouts.
    bool HasPendingEscalation(TeamId team, uint32 zoneId);
    bool ClaimEscalation(TeamId team, uint32 zoneId, WpvpDefenseEntry& out);

    // A fresh, called-out entry this bot hasn't rolled response dice for yet
    // and is not hopelessly outleveled by (level + slack >= attacker level).
    bool FindRespondable(TeamId team, uint8 botLevel, ObjectGuid botGuid, WpvpDefenseEntry& out);
    bool FindByZone(TeamId team, uint32 zoneId, WpvpDefenseEntry& out);

    // One response roll per bot per attacker, ever: whether the dice pass or
    // fail, the bot doesn't roll for this ganker again.
    bool TryClaimResponseRoll(ObjectGuid bot, ObjectGuid attacker);

private:
    WpvpDefenseBoard() = default;

    bool IsRespondable(WpvpDefenseEntry const& entry, uint32 now) const;
    void Prune(uint32 now);

    std::mutex _mutex;
    std::unordered_map<ObjectGuid, WpvpDefenseEntry> _entries;
    std::unordered_set<uint64> _responseRolls;
    uint32 _lastPruneMs{0};
};

// Set up a defense response for this bot: reuse the wpvp excursion status in
// defend mode, aimed at the given spot. Same-zone bots just run over; remote
// bots wait out a distance-scaled travel delay, then guarded-teleport in.
bool StartWpvpDefenseResponse(PlayerbotAI* botAI, uint32 zoneId, WorldPosition const& target, ObjectGuid attacker);

// A ganker's spree crossed the escalation threshold and this bot is in the
// zone: claim the one WorldDefense shout.
class WpvpEscalationCalloutTrigger : public Trigger
{
public:
    WpvpEscalationCalloutTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp escalation callout", 10) {}

    bool IsActive() override;
};

class WpvpEscalationCalloutAction : public Action
{
public:
    WpvpEscalationCalloutAction(PlayerbotAI* botAI) : Action(botAI, "wpvp escalation callout") {}

    bool Execute(Event event) override;
};

// An idle same-faction bot hears about a ganker on the board and may decide
// (one dice roll per ganker) to travel there and defend.
class WpvpDefenseResponseTrigger : public Trigger
{
public:
    WpvpDefenseResponseTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp defense response", 10) {}

    bool IsActive() override;
};

class WpvpDefenseResponseAction : public Action
{
public:
    WpvpDefenseResponseAction(PlayerbotAI* botAI) : Action(botAI, "wpvp defense response") {}

    bool Execute(Event event) override;
};

// "wpvp defend [zone]" chat command: explicit defense order, whisperable by
// real players and callable by mod-llm's go_defend tool. Resolves the zone
// to a board entry (precise ganker position) or a wpvp hub anchor.
class WpvpDefendCommandAction : public Action
{
public:
    WpvpDefendCommandAction(PlayerbotAI* botAI) : Action(botAI, "wpvp defend") {}

    bool Execute(Event event) override;
};

#endif
