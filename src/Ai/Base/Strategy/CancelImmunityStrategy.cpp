/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "CancelImmunityStrategy.h"

#include "Playerbots.h"

void CancelImmunityStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Above every cast: inside Ice Block, Dispersion or Divine Intervention nothing else can
    // execute anyway, and a healed-up bubble should drop before the next heal is chosen.
    switch (botAI->GetBot()->getClass())
    {
        case CLASS_MAGE:
            triggers.push_back(new TriggerNode("ice block outlived",
                { NextAction("cancel ice block", ACTION_EMERGENCY + 5) }));
            break;
        case CLASS_PALADIN:
            triggers.push_back(new TriggerNode("divine shield outlived",
                { NextAction("cancel divine shield", ACTION_EMERGENCY + 5) }));
            break;
        case CLASS_PRIEST:
            triggers.push_back(new TriggerNode("dispersion outlived",
                { NextAction("cancel dispersion", ACTION_EMERGENCY + 5) }));
            break;
        // Not an emergency and not free - it is a cast that hands the group back a live mob, so it
        // sits with the rest of the combat kit rather than above it. Nothing competes with it in
        // practice: the trigger only fires once the attacker list is empty.
        case CLASS_WARLOCK:
            triggers.push_back(new TriggerNode("banish outlived",
                { NextAction("cancel banish", ACTION_HIGH) }));
            break;
        default:
            break;
    }

    // Cast on the bot by a paladin - any class can be wearing these.
    triggers.push_back(new TriggerNode("divine intervention outlived",
        { NextAction("cancel divine intervention", ACTION_EMERGENCY + 5) }));
    triggers.push_back(new TriggerNode("hand of protection outlived",
        { NextAction("cancel hand of protection", ACTION_EMERGENCY + 5) }));
}
