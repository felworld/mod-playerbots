/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DungeonHoldStrategy.h"

#include "Action.h"
#include "DungeonHoldValues.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Strategy.h"

#include <unordered_set>

namespace
{
    // Picking a target and turning to face it is not offense. The bot lines its target up through the
    // hold - AttackAction::Attack keeps the swing back on its own - so the fight starts on the tick
    // the hold releases instead of a target acquisition later. An explicit pull command outranks the
    // hold the same way it outranks "wait for attack".
    std::unordered_set<std::string> const HoldExemptActions = {
        "dps assist",
        "dps aoe",
        "tank assist",
        "attack least hp target",
        "attack rti target",
        "attack my target",
        "set facing",
        "pull my target",
        "pull rti target",
        "reach pull",
        "pull start",
        "pull action",
        "pull end",
    };
}

void DungeonHoldStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Both handlers hang off level-based triggers on purpose: the class strategies dispatch the pet
    // and start the swing from "target changed", which fired long before the tank picked the mob up.
    triggers.push_back(new TriggerNode("dungeon hold release", { NextAction("dungeon hold attack", 50.0f) }));
    triggers.push_back(new TriggerNode("pet hold release", { NextAction("pet assist", 15.0f) }));
}

void DungeonHoldStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new DungeonHoldMultiplier(botAI));
}

float DungeonHoldMultiplier::GetValue(Action* action)
{
    if (!IsDungeonHoldActive(botAI))
        return 1.0f;

    if (HoldExemptActions.contains(action->getName()))
        return 1.0f;

    // Suppression is by target rather than by action name: whatever the action is, it is held only
    // when it points at a mob the tank has not picked up. Heals, buffs, self-defence, follow and
    // formation movement all resolve to a friendly unit or to the bot itself and pass untouched.
    // The value can be missing - an action is free to name a target value nothing registers.
    Value<Unit*>* targetValue = action->GetTargetValue();
    if (!targetValue)
        return 1.0f;

    Unit* target = targetValue->Get();
    if (!target)
        return 1.0f;

    return ShouldHoldForTank(botAI, target) ? 0.0f : 1.0f;
}
