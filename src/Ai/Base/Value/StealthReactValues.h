/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_STEALTHREACTVALUES_H
#define _PLAYERBOT_STEALTHREACTVALUES_H

#include "ObjectGuid.h"
#include "Timer.h"
#include "Value.h"

#include <unordered_map>

class Player;
class PlayerbotAI;
class Unit;

// True when the bot's stealth detection reaches the target at its current
// distance - the core math from WorldObject::CanDetectStealthOf minus the
// front-arc test: the client's stealth-detect ping isn't directional, so
// the bot's awareness isn't either.
bool CanDetectStealth360(Player* bot, Unit* target);

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

#endif
