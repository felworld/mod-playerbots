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

    // Relevance 60: sweeping a vanished enemy's last spot is a deliberate
    // hunt, not a reflex - it outranks idle wandering and RPG business but
    // yields to the reflexes above, to rescues, and (in the combat engine,
    // where this strategy also runs) to emergency self-care. The action's
    // isUseful additionally stands down whenever a perceivable target
    // exists - fighting the seen beats poking at shadows.
    triggers.push_back(new TriggerNode("stealth suspicion", {
        NextAction("flush stealther", 60.0f) }));
}
