/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_STEALTHREACTTRIGGERS_H
#define _PLAYERBOT_STEALTHREACTTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

class StealtherSpottedTrigger : public Trigger
{
public:
    // Interval 1: the window in which a walking stealther crosses detection
    // range is short - a skipped tick is a missed heart-jump.
    StealtherSpottedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "stealther spotted", 1) {}

    bool IsActive() override;
};

class StealthSpotEmoteTrigger : public Trigger
{
public:
    StealthSpotEmoteTrigger(PlayerbotAI* botAI) : Trigger(botAI, "stealth spot emote", 1) {}

    bool IsActive() override;
};

class StealthSuspicionTrigger : public Trigger
{
public:
    // Interval 1 keeps the suspicion value's perception ledger warm on
    // every tick - the ledger is what turns a mid-fight Vanish into a
    // suspicion at the right spot, so it can't afford stale scans.
    StealthSuspicionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "stealth suspicion", 1) {}

    bool IsActive() override;
};

#endif
