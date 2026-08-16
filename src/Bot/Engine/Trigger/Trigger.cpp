/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Trigger.h"
#include "AiObjectContext.h"
#include "Event.h"
#include "Random.h"
#include "Timer.h"
#include "Unit.h"

Trigger::Trigger(PlayerbotAI* botAI, std::string const name, int32 checkInterval)
    : AiNamedObject(botAI, name),
      checkInterval(checkInterval == 1 ? 1 : (checkInterval < 100 ? checkInterval * 1000 : checkInterval)),
      lastCheckTime(0)
{
}

Event Trigger::Check()
{
    if (!IsActive())
    {
        reactionArmedAt = 0;
        reactionTarget.Clear();
        return Event();
    }

    if (!ReactionDelayElapsed(getMSTime()))
        return Event();

    Event event(getName());
    return event;
}

// Human-reaction triggers (interrupts, dispels, emergency heals) fire only
// after a jittered delay sampled when the trigger flips active; a missed roll
// waits out the whole window, then re-rolls if the situation still holds.
bool Trigger::ReactionDelayElapsed(uint32 now)
{
    ReactionCategory category = GetReactionCategory();
    if (category == REACTION_NONE || !sPlayerbotAIConfig.reactionDelayMax[category])
        return true;

    Unit* target = GetTarget();
    ObjectGuid targetGuid = target ? target->GetGUID() : ObjectGuid::Empty;

    // Re-arm on a target change so a second victim doesn't inherit the first
    // victim's already-elapsed delay as an instant free reaction
    if (!reactionArmedAt || targetGuid != reactionTarget)
        ArmReactionLatch(now, targetGuid);

    if (getMSTimeDiff(reactionArmedAt, now) < reactionDelay)
        return false;

    if (reactionMissed)
    {
        ArmReactionLatch(now, targetGuid);
        return false;
    }

    return true;
}

void Trigger::ArmReactionLatch(uint32 now, ObjectGuid targetGuid)
{
    ReactionCategory category = GetReactionCategory();
    uint32 maxDelay = sPlayerbotAIConfig.reactionDelayMax[category];
    uint32 minDelay = std::min(sPlayerbotAIConfig.reactionDelayMin[category], maxDelay);

    reactionTarget = targetGuid;
    reactionArmedAt = std::max(now, 1u);
    reactionMissed = roll_chance_i(int32(sPlayerbotAIConfig.reactionMissChance[category]));
    reactionDelay = reactionMissed ? maxDelay : urand(minDelay, maxDelay);
}

Value<Unit*>* Trigger::GetTargetValue() { return context->GetValue<Unit*>(GetTargetName()); }

Unit* Trigger::GetTarget() { return GetTargetValue()->Get(); }

bool Trigger::needCheck(uint32 now)
{
    if (checkInterval < 2)
        return true;

    if (!lastCheckTime || now - lastCheckTime >= uint32(checkInterval))
    {
        lastCheckTime = now;
        return true;
    }

    return false;
}
