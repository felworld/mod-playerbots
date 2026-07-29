/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "StealthReactTriggers.h"

#include "Playerbots.h"
#include "StealthReactValues.h"

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
