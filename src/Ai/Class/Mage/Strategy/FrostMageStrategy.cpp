/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "FrostMageStrategy.h"
#include "Playerbots.h"

FrostMageStrategy::FrostMageStrategy(PlayerbotAI* botAI) : GenericMageStrategy(botAI)
{
    // No custom ActionNodeFactory needed
}

// ===== Default Actions =====
std::vector<NextAction> FrostMageStrategy::getDefaultActions()
{
    return {
        NextAction("frostbolt", 5.4f),
        NextAction("ice lance", 5.3f),   // cast during movement
        NextAction("fire blast", 5.2f),  // cast during movement if ice lance is not learned
        NextAction("shoot", 5.1f),
        NextAction("fireball", 5.0f)
    };
}

// ===== Trigger Initialization ===
void FrostMageStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericMageStrategy::InitTriggers(triggers);

    // Pet/Defensive triggers
    triggers.push_back(
        new TriggerNode(
            "no pet",
            {
                NextAction("summon water elemental", 30.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "has pet",
            {
                NextAction("toggle pet spell", 60.0f),
                NextAction("set pet stance", 59.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "new pet",
            {
                NextAction("set pet stance", 60.0f)
            }
        )
    );
    // Keep the elemental on the bot's target instead of letting core PetAI pick its own attacker.
    triggers.push_back(
        new TriggerNode(
            "target changed",
            {
                NextAction("pet assist", 15.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "medium health",
            {
                NextAction("ice barrier", 29.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "being attacked",
            {
                NextAction("ice barrier", 29.0f)
            }
        )
    );

    // Proc/Freeze triggers
    triggers.push_back(
        new TriggerNode(
            "brain freeze",
            {
                NextAction("frostfire bolt", 19.5f)
            }
        )
    );
    // Everything a frost mage casts at a frozen target shatters; Ice Lance is the instant one,
    // so it picks up the shatter crit whenever Frostbolt is off the table (moving, or on the
    // way to a Deep Freeze).
    triggers.push_back(
        new TriggerNode(
            "fingers of frost",
            {
                NextAction("deep freeze", 19.0f),
                NextAction("frostbolt", 18.0f),
                NextAction("ice lance", 17.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "frostbite on target",
            {
                NextAction("deep freeze", 19.0f),
                NextAction("frostbolt", 18.0f),
                NextAction("ice lance", 17.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "frost nova on target",
            {
                NextAction("deep freeze", 19.0f),
                NextAction("frostbolt", 18.0f),
                NextAction("ice lance", 17.5f)
            }
        )
    );

    // Presence of Mind is spent on the longest cast in the book, not on whatever filler comes next
    triggers.push_back(
        new TriggerNode(
            "presence of mind active",
            {
                NextAction("frostbolt", 26.0f)
            }
        )
    );

    // Instant fillers while moving (the engine refuses cast-time spells, so without these the
    // bot just auto-attacks)
    triggers.push_back(
        new TriggerNode(
            "moving filler",
            {
                NextAction("cone of cold", 5.95f),
                NextAction("ice lance", 5.9f),
                NextAction("fire blast", 5.85f)
            }
        )
    );
}
