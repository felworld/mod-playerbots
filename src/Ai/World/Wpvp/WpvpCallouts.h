#ifndef PLAYERBOTS_WPVPCALLOUTS_H
#define PLAYERBOTS_WPVPCALLOUTS_H

#include <mutex>
#include <unordered_map>

#include "Action.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "WpvpDefense.h"

class Player;
class PlayerbotAI;

// Cross-bot dedup for LocalDefense invader callouts. Bot AI runs in
// map-update worker context, so all state is mutex-guarded. CanReport is a
// cheap peek for the trigger; TryReport is the check-and-record the action
// uses, so exactly one bot wins when several spot the same invader at once.
class WpvpCalloutThrottle
{
public:
    static WpvpCalloutThrottle& instance()
    {
        static WpvpCalloutThrottle instance;
        return instance;
    }

    bool CanReport(TeamId team, uint32 zoneId, ObjectGuid attacker);
    bool TryReport(TeamId team, uint32 zoneId, ObjectGuid attacker);

private:
    WpvpCalloutThrottle() = default;

    bool Check(uint64 zoneKey, ObjectGuid attacker, uint32 now) const;
    void Prune(uint32 now);

    std::mutex _mutex;
    std::unordered_map<uint64, uint32> _lastZoneCallout;
    std::unordered_map<ObjectGuid, uint32> _lastAttackerCallout;
    uint32 _lastPruneMs{0};
};

// The opposing player this bot would report, and what they were seen doing.
// Flagged presence alone is not it: on a PvP-type realm everyone in a
// contested zone is flagged, so the flag says nothing about intent. An enemy
// qualifies by being seen in combat with the defending side - the bot,
// another player, or friendly NPCs - or by being a still-fresh ganker the
// defense channels already called out (Prowling). Enemies the bot outlevels
// by the gank gap are skipped either way: nothing to raise an alarm over.
// Returns false when there is nothing to report.
struct WpvpIntruderSighting
{
    Player* intruder{nullptr};
    WpvpCalloutActivity activity{WpvpCalloutActivity::Prowling};
    std::string victimName;  // AttackingPlayer only
};

bool FindWpvpIntruder(PlayerbotAI* botAI, WpvpIntruderSighting& out);

// A random defender bot (not itself on an excursion) sees an invader and the
// throttle window is open.
class WpvpDefenseCalloutTrigger : public Trigger
{
public:
    WpvpDefenseCalloutTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp defense callout", 10) {}

    bool IsActive() override;
};

// Shout the invader's name and location into LocalDefense.
class WpvpDefenseCalloutAction : public Action
{
public:
    WpvpDefenseCalloutAction(PlayerbotAI* botAI) : Action(botAI, "wpvp defense callout") {}

    bool Execute(Event event) override;
};

#endif
