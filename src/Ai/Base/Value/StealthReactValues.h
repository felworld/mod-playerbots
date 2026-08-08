/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_STEALTHREACTVALUES_H
#define _PLAYERBOT_STEALTHREACTVALUES_H

#include "ObjectGuid.h"
#include "Position.h"
#include "Timer.h"
#include "Value.h"

#include <string>
#include <unordered_map>

class Player;
class PlayerbotAI;
class Unit;

// True when the bot's stealth detection reaches the target at its current
// distance - the core math from WorldObject::CanDetectStealthOf minus the
// front-arc test: the client's stealth-detect ping isn't directional, so
// the bot's awareness isn't either.
bool CanDetectStealth360(Player* bot, Unit* target);

// The distance at which the seer's front-arc stealth detection reaches the
// stealther - the range half of WorldObject::CanDetectStealthOf. Beyond it
// (or anywhere out of the seer's front 180°) the stealther is invisible.
// Returns MAX_PLAYER_STEALTH_DETECT_RANGE when the seer pierces stealth
// outright (a detect-stealth aura such as Flare).
float StealthDetectionRange(Unit const* seer, Unit const* stealther);

// An emote owed to a stealther the bot just startled at, stashed by the
// startle action to fire a beat later. timeMs of 0 means "none".
struct StealthSpotEvent
{
    ObjectGuid stealther;
    uint32 timeMs = 0;
};

// The emote fires on the first AI check after the startle pause and goes
// stale shortly after - a wave at someone long gone reads as a glitch.
constexpr uint32 STEALTH_SPOT_EMOTE_WINDOW_MS = 6 * 1000;

// The nearby stealthed player this bot just detected and hasn't reacted to
// yet, or nullptr. Owns the per-stealther cooldown that turns a rogue
// circling the bot into one heart-jump instead of a seizure loop.
class StealtherSpottedValue : public UnitCalculatedValue
{
public:
    StealtherSpottedValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "stealther spotted") {}

    Unit* Calculate() override;

    void MarkReacted(ObjectGuid stealtherGuid);

private:
    std::unordered_map<ObjectGuid, uint32> _cooldownEndMs;
    uint32 _lastPruneMs = 0;
};

// The enemy this bot has reason to believe is hiding nearby: it perceived
// them - saw them plainly, or had a detection ping on their stealth - and
// then lost them while they carried a stealth or invisibility aura. A
// rogue Vanishing mid-fight, a duel opponent stealthing in the countdown,
// a spotted sneak slipping back out of detection range. timeMs of 0 means
// no suspicion.
struct StealthSuspicion
{
    ObjectGuid stealther;
    std::string stealtherName;
    Position lastKnown;
    uint32 timeMs = 0;
    // The one-per-suspicion patience roll (StealthFlushChance): a failed
    // roll is a bot that shrugs and moves on instead of sweeping the spot.
    bool flushApproved = false;
};

// Watches enemy players the bot can currently perceive and turns a
// perception loss into a StealthSuspicion when the vanished party is in
// fact hidden (stealth/invisibility aura) rather than simply gone. The
// suspicion lives for StealthFlushSeconds, and clears early if the
// stealther becomes perceivable again - from then on direct targeting is
// the right tool, not area flushing.
class StealthSuspicionValue : public CalculatedValue<StealthSuspicion>
{
public:
    StealthSuspicionValue(PlayerbotAI* botAI) : CalculatedValue<StealthSuspicion>(botAI, "stealth suspicion") {}

    StealthSuspicion Calculate() override;

    // Spacing between flush casts, so a mage sweeps the spot with a few
    // Arcane Explosions over the window instead of one per AI tick.
    bool FlushCastReady() const;
    void MarkFlushCast();

private:
    struct Perceived
    {
        Position pos;
        uint32 lastSeenMs = 0;
    };

    std::unordered_map<ObjectGuid, Perceived> _perceived;
    StealthSuspicion _suspicion;
    uint32 _lastFlushMs = 0;
};

#endif
