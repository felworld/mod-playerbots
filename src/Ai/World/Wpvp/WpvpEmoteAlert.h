/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_WPVPEMOTEALERT_H
#define PLAYERBOTS_WPVPEMOTEALERT_H

#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "Action.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "TravelMgr.h"
#include "Trigger.h"

class Player;
class PlayerbotAI;

// One "somebody emoted at that enemy" sighting: where the enemy and the
// emoter stood when it happened, and who already answered.
struct WpvpEmoteAlertEntry
{
    ObjectGuid target;
    ObjectGuid emoter;
    TeamId alertedTeam{TEAM_NEUTRAL};
    uint32 zoneId{0};
    WorldPosition targetPos;
    WorldPosition emoterPos;
    uint8 targetLevel{0};
    uint32 postedMs{0};
    uint32 updatedMs{0};
    std::unordered_set<ObjectGuid> responded;
};

// A targeted text emote landed on someone: called from the text-emote hook
// with the raw target guid. Validates the pair (opposing teams, PvP-flagged
// enemy player within vision range, no BG/arena/prohibited zone) and posts
// the sighting for witnesses.
void NoteTargetedEmoteAtEnemy(Player* emoter, ObjectGuid targetGuid);

// Bulletin board of fresh emote sightings, keyed by the enemy. Producer: the
// text-emote hook (any map-update thread). Consumers: witness bots' triggers.
// All state is mutex-guarded.
class WpvpEmoteAlertBoard
{
public:
    static WpvpEmoteAlertBoard& instance()
    {
        static WpvpEmoteAlertBoard instance;
        return instance;
    }

    void Post(Player* emoter, Player* target);

    // A fresh sighting this bot witnessed (was inside the text-emote listen
    // range of the emoter, so the emote was on its screen), hasn't answered
    // yet, and isn't hopelessly outleveled by. The emoter itself never
    // qualifies - the feature informs bystanders, it doesn't make pointing
    // a self-command to charge.
    bool FindAlertFor(Player* bot, WpvpEmoteAlertEntry& out);

    // Mark the bot as having answered this sighting; false if it already did
    // or the sighting expired. Every witness answers at most once per
    // sighting, deterministically - the emote is the dice roll.
    bool ClaimResponse(ObjectGuid bot, ObjectGuid target);

private:
    WpvpEmoteAlertBoard() = default;

    void Prune(uint32 now);

    std::mutex _mutex;
    std::unordered_map<ObjectGuid, WpvpEmoteAlertEntry> _entries;
};

// A friendly emoted at an enemy where this bot could see it: converge on the
// spot and hunt them, via the defense-response travel machinery.
class WpvpEmoteAlertTrigger : public Trigger
{
public:
    WpvpEmoteAlertTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp emote alert", 5) {}

    bool IsActive() override;
};

class WpvpEmoteAlertAction : public Action
{
public:
    WpvpEmoteAlertAction(PlayerbotAI* botAI) : Action(botAI, "wpvp emote alert") {}

    bool Execute(Event event) override;
};

#endif
