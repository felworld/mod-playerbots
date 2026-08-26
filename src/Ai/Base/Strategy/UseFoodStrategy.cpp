/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UseFoodStrategy.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"

void UseFoodStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    Strategy::InitTriggers(triggers);
    if (botAI->HasCheat(BotCheatMask::food))
    {
        triggers.push_back(new TriggerNode("medium health", { NextAction("food", 3.0f) }));
        triggers.push_back(new TriggerNode("high mana", { NextAction("drink", 3.0f) }));
    }
    else
    {
        triggers.push_back(new TriggerNode("low health", { NextAction("food", 3.0f) }));
        triggers.push_back(new TriggerNode("low mana", { NextAction("drink", 3.0f) }));
    }

    // The master sitting down is the group taking a breather: take the opportunity to top off even
    // if the usual low-resource bar was not hit. The trigger staggers each bot by its own delay, so
    // the party does not drop to the floor in unison - bots never sit for any other reason.
    triggers.push_back(new TriggerNode("master is resting", { NextAction("drink", 3.2f), NextAction("food", 3.1f) }));

    // In battlegrounds eating runs on short AI ticks instead of one long sleep; this hold keeps
    // the bot seated while it is safe, and simply not firing lets higher-priority actions
    // (objectives, combat) stand the bot up and break the regen aura.
    //
    // The hold outranks the whole loot chain ("can loot" @8.0 down to "add all loot" @5.0):
    // every movement action force-stands the bot, so a corpse or a herb noticed mid-drink used
    // to yank a healer off the water over and over and it never reached full mana. The trigger
    // only fires while the bot is already consuming and goes false in combat or at 100%, so the
    // hold self-terminates and looting resumes right after the meal.
    triggers.push_back(new TriggerNode("consuming food or drink", { NextAction("continue eating", 8.5f) }));
}
