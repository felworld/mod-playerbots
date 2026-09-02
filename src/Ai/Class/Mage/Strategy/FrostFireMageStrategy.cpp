/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "FrostFireMageStrategy.h"
#include "Playerbots.h"

FrostFireMageStrategy::FrostFireMageStrategy(PlayerbotAI* botAI) : GenericMageStrategy(botAI)
{
    // No custom ActionNodeFactory needed
}

// ===== Default Actions =====
std::vector<NextAction> FrostFireMageStrategy::getDefaultActions()
{
    return {
        NextAction("frostfire bolt", 5.2f),
        NextAction("fire blast", 5.1f),  // cast during movement
        NextAction("shoot", 5.0f)
    };
}

// ===== Trigger Initialization =====
void FrostFireMageStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericMageStrategy::InitTriggers(triggers);

    // Debuff Triggers
    triggers.push_back(
        new TriggerNode(
            "improved scorch",
            {
                NextAction("scorch", 19.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "living bomb",
            {
                NextAction("living bomb", 18.5f)
            }
        )
    );

    // Proc Trigger
    triggers.push_back(
        new TriggerNode(
            "hot streak",
            {
                NextAction("pyroblast", 25.0f)
            }
        )
    );

    // Presence of Mind belongs on a Pyroblast, but a free Hot Streak one comes first - an
    // instant cast does not consume the charge
    triggers.push_back(
        new TriggerNode(
            "presence of mind active",
            {
                NextAction("pyroblast", 24.5f),
                NextAction("frostfire bolt", 24.0f)
            }
        )
    );

    // Sheep the player we just engaged, then break it with a full Pyroblast
    triggers.push_back(
        new TriggerNode(
            "polymorph opener",
            {
                NextAction("polymorph on target", 30.0f)
            }
        )
    );
    // Polymorph invalidates the current target the instant it lands, so the payoff has to
    // outrank the "drop target" reaction at 99 or the bot walks away from its own setup
    triggers.push_back(
        new TriggerNode(
            "polymorphed opponent",
            {
                NextAction("pyroblast on cc target", 99.5f)
            }
        )
    );

    // Instant fillers while moving (the engine refuses cast-time spells, so without these the
    // bot just auto-attacks)
    triggers.push_back(
        new TriggerNode(
            "moving filler",
            {
                NextAction("fire blast", 5.9f),
                NextAction("ice lance", 5.85f),
                NextAction("cone of cold", 5.8f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "enemy too close for spell",
            {
                NextAction("dragon's breath", ACTION_INTERRUPT + 1)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "enemy is close",
            {
                NextAction("blast wave", ACTION_INTERRUPT)
            }
        )
    );
}
