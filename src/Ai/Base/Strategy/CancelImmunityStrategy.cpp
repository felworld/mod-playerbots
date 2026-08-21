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
        default:
            break;
    }

    // Cast on the bot by a paladin - any class can be wearing these.
    triggers.push_back(new TriggerNode("divine intervention outlived",
        { NextAction("cancel divine intervention", ACTION_EMERGENCY + 5) }));
    triggers.push_back(new TriggerNode("hand of protection outlived",
        { NextAction("cancel hand of protection", ACTION_EMERGENCY + 5) }));
}
