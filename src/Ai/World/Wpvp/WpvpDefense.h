#ifndef PLAYERBOTS_WPVPDEFENSE_H
#define PLAYERBOTS_WPVPDEFENSE_H

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

// What the reported enemy was actually seen doing - the callout wording has
// to match it, or bots shout "attacking Greenpaw Village" about someone
// grinding furbolgs there.
enum class WpvpCalloutActivity : uint8
{
    AttackingPlayer,  // in combat with a defending-side player (or their pet)
    AttackingNpcs,    // in combat with NPCs friendly to the defenders (guards, civilians)
    Prowling,         // not seen fighting, but a still-fresh, already-called-out ganker
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
    // FirstCallout only: what the attacker was seen doing, and - for
    // AttackingPlayer - who they were fighting (the speaker's own name when
    // the speaker is the one attacked).
    WpvpCalloutActivity activity{WpvpCalloutActivity::AttackingNpcs};
    std::string victimName;
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
    std::vector<ObjectGuid> victims;  // players this attacker killed (capped, oldest dropped)
    uint8 maxVictimLevel{0};      // highest-level victim so far (gank-vs-even-fight classifier)
    bool escalationPending{false};
    bool escalated{false};        // the one WorldDefense shout has been made
    uint32 avengedDeaths{0};      // deaths to someone who was NOT one of their victims
    uint32 reinforceArmedMs{0};   // 0 until the attacker's faction gets its one reinforcement wave
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
    // arming a WorldDefense escalation at the configured threshold. Only
    // genuine gank kills (victim a full gank gap below the attacker) feed
    // the tally - even fights never escalate. The kill also counts as
    // evidence for the KILLER's side: tracked gankers of the other side in
    // the same zone whom the killer is a level match for are officially
    // contested - their pending escalations are cancelled and their tallies
    // reset, so mid-battle "ganks" silence the enemy's alarm rather than
    // feed a shouting match.
    void RecordKill(Player* attacker, Player* victim);

    // A tracked ganker died: the spree is contested, the tally resets and
    // any not-yet-claimed escalation is cancelled. Deaths to someone who was
    // never one of their victims (outside help - bot defenders or a real
    // player riding in) count toward arming the one reinforcement wave from
    // the ganker's own faction.
    void RecordAttackerDeath(Player* attacker, ObjectGuid killer);

    // From the defend-mode dwell loop: a live defender is in the zone with
    // the tracked attacker. When the defender is on the defending team
    // (reinforcers dwell on the attacker's side) and within the gank gap of
    // the attacker's level, the fight counts as handled: any pending
    // WorldDefense escalation is cancelled and the spree tally resets - the
    // shout must be re-earned from zero.
    void NoteDefenderOnScene(ObjectGuid attacker, TeamId team, uint8 defenderLevel);

    // Escalation is claimed only by an outmatched eyewitness - a victim of
    // the spree still in the ganker's zone, or an on-screen bystander -
    // either way a full gank gap below the ganker (the trigger checks that);
    // the board just hands out pending entries and takes the atomic claim,
    // so exactly one bot shouts. One shout per battlefield: a successful
    // claim stamps a per-team-per-zone cooldown (EscalationWindow), and
    // while it runs further pending pleas about that zone are cancelled,
    // not queued.
    std::vector<WpvpDefenseEntry> PendingEscalations(TeamId team);
    bool ClaimEscalation(TeamId team, ObjectGuid attacker, WpvpDefenseEntry& out);

    // A fresh, called-out entry this bot hasn't rolled response dice for yet
    // and is not hopelessly outleveled by (level + slack >= attacker level).
    bool FindRespondable(TeamId team, uint8 botLevel, ObjectGuid botGuid, WpvpDefenseEntry& out);

    // The attacker has a fresh entry against this team that some channel
    // already named: sighting them again is news worth repeating even when
    // they aren't seen fighting. An attacker nobody announced - including a
    // failed gank, which leaves no entry at all - stays anonymous.
    bool IsKnownThreat(ObjectGuid attacker, TeamId team);
    bool FindByZone(TeamId team, uint32 zoneId, WpvpDefenseEntry& out);

    // Any tracked attacker - either side, called out or not - whose latest
    // kill or callout placed them in this zone inside the window: "a fight
    // just happened here".
    bool RecentActivityInZone(uint32 mapId, uint32 zoneId, uint32 windowMs);

    // A tracked attacker on THIS bot's team whose reinforcement wave is armed
    // and fresh - the backchannel "friends, I'm getting swarmed" ask. Same
    // one-roll-per-bot-per-attacker dice set as defense responses (a bot is
    // only ever on one side of a given ganker).
    bool FindReinforceable(TeamId team, uint8 botLevel, ObjectGuid botGuid, WpvpDefenseEntry& out);

    // One response roll per bot per attacker, ever: whether the dice pass or
    // fail, the bot doesn't roll for this ganker again.
    bool TryClaimResponseRoll(ObjectGuid bot, ObjectGuid attacker);

private:
    WpvpDefenseBoard() = default;

    bool IsRespondable(WpvpDefenseEntry const& entry, uint32 now) const;
    bool CapableDefenseActive(WpvpDefenseEntry const& entry, uint32 now) const;
    bool EscalationCoolingDown(TeamId team, uint32 zoneId, uint32 now) const;
    void Prune(uint32 now);

    std::mutex _mutex;
    std::unordered_map<ObjectGuid, WpvpDefenseEntry> _entries;
    std::unordered_set<uint64> _responseRolls;
    // Last WorldDefense escalation shout per (defending team, zone): one
    // call per battlefield per EscalationWindow.
    std::unordered_map<uint64, uint32> _escalationShoutMs;
    uint32 _lastPruneMs{0};
};

// Set up a defense response for this bot: reuse the wpvp excursion status in
// defend mode, aimed at the given spot. Same-zone bots just run over; remote
// bots wait out a distance-scaled travel delay, then guarded-teleport in.
bool StartWpvpDefenseResponse(PlayerbotAI* botAI, uint32 zoneId, WorldPosition const& target, ObjectGuid attacker);

// World PvP is happening around this bot, or just was: a board entry in its
// zone from the last couple of minutes, or a PvP-flagged enemy player inside
// vision range right now. Downtime leisure - dueling for fun - reads wrong
// next to a battlefield; use this to hold it until things quiet down.
bool WpvpHappeningNearby(Player* bot);

// A ganker's spree crossed the escalation threshold and this bot knows it
// first-hand (it died to them and is still in their zone, or can see them
// right now) while sitting a full gank gap below their level: claim the one
// WorldDefense shout.
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

// A faction-mate's gank went sideways - defenders keep killing them - and
// this idle bot may decide (one dice roll per ganker) to ride in and back
// them up. No chat: we assume the ask happened over some backchannel.
class WpvpReinforceTrigger : public Trigger
{
public:
    WpvpReinforceTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp reinforce", 10) {}

    bool IsActive() override;
};

class WpvpReinforceAction : public Action
{
public:
    WpvpReinforceAction(PlayerbotAI* botAI) : Action(botAI, "wpvp reinforce") {}

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
