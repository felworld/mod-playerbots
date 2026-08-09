/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "StealthReactTriggers.h"

#include "Playerbots.h"
#include "StealthReactValues.h"

namespace
{
    // The human-scale beat between the forced turn and the reaction: long
    // enough that a Distract thrown from inside ~12yd usually still buys
    // the opener, short enough that one thrown from 20+ gets punished.
    constexpr uint32 DISTRACT_REACT_DELAY_MIN_MS = 1000;
    constexpr uint32 DISTRACT_REACT_DELAY_MAX_MS = 2500;
}

bool StealtherSpottedTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.enableStealthReactions)
        return false;

    if (bot->IsInCombat())
        return false;

    return AI_VALUE(Unit*, "stealther spotted") != nullptr;
}

bool StealthSpotEmoteTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.enableStealthReactions)
        return false;

    if (bot->IsInCombat())
        return false;

    StealthSpotEvent event = AI_VALUE(StealthSpotEvent, "pending stealth emote");
    return event.timeMs && getMSTimeDiff(event.timeMs, getMSTime()) < STEALTH_SPOT_EMOTE_WINDOW_MS;
}

bool StealthSuspicionTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.enableStealthReactions || !sPlayerbotAIConfig.stealthFlushChance)
        return false;

    // A bot that is itself hiding sweeps nothing - it stays put.
    if (bot->HasStealthAura())
        return false;

    // Unlike the startle, this trigger deliberately runs in combat too:
    // querying the value keeps its perception ledger warm through fights,
    // and a mid-fight Vanish is exactly when Consecration on the spot
    // matters most.
    StealthSuspicion suspicion = AI_VALUE(StealthSuspicion, "stealth suspicion");
    return suspicion.timeMs && suspicion.flushApproved;
}

bool DistractedTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.enableStealthReactions || !sPlayerbotAIConfig.distractSuspicionChance)
        return false;

    // The state clearing - the timer ran out, combat broke it, or the
    // shake-off action ended it - is what closes an incident; the next
    // distract rolls fresh.
    if (!bot->HasUnitState(UNIT_STATE_DISTRACTED))
    {
        _incidentMs = 0;
        return false;
    }

    if (!bot->IsAlive())
        return false;

    uint32 const now = getMSTime();
    if (!_incidentMs)
    {
        _incidentMs = now;
        _sawThrough = roll_chance_i(sPlayerbotAIConfig.distractSuspicionChance);
        _delayMs = urand(DISTRACT_REACT_DELAY_MIN_MS, DISTRACT_REACT_DELAY_MAX_MS);
    }

    return _sawThrough && getMSTimeDiff(_incidentMs, now) >= _delayMs;
}
