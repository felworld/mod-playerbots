/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UseEngineeringStrategy.h"

void UseEngineeringStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    Strategy::InitTriggers(triggers);

    // Just below class interrupts, so a real kick wins when both are available.
    triggers.push_back(new TriggerNode(
        "grenade interrupt", { NextAction("grenade interrupt", ACTION_INTERRUPT - 1) }));
    triggers.push_back(new TriggerNode(
        "sapper charge", { NextAction("sapper charge", ACTION_HIGH + 8) }));
    triggers.push_back(new TriggerNode(
        "target dummy", { NextAction("target dummy", ACTION_HIGH + 9) }));
    triggers.push_back(new TriggerNode(
        "throw explosives", { NextAction("throw explosives", ACTION_HIGH + 2) }));
    triggers.push_back(new TriggerNode(
        "explosive sheep", { NextAction("explosive sheep", ACTION_HIGH + 1) }));
    triggers.push_back(new TriggerNode(
        "rocket boots", { NextAction("rocket boots", ACTION_MOVE + 5) }));
    triggers.push_back(new TriggerNode(
        "glove tinker", { NextAction("glove tinker", ACTION_HIGH) }));
}
