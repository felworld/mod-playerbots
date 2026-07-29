/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "StealthReactStrategy.h"

#include "Playerbots.h"

void StealthReactStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Relevance 88-89: a startle is involuntary - it cuts ahead of social
    // niceties (social buff at 85-87) but yields to a rescue (bystander
    // assist at 90), and stays under 100 so bots with no real player
    // nearby (Engine skips <100 nodes while "minimal") don't jump at
    // ghosts nobody would see. The follow-up emote sits just under the
    // startle so a fresh detection always outranks a leftover wave.
    triggers.push_back(new TriggerNode("stealther spotted", {
        NextAction("startle at stealther", 89.0f) }));
    triggers.push_back(new TriggerNode("stealth spot emote", {
        NextAction("stealth spot emote", 88.0f) }));
}
