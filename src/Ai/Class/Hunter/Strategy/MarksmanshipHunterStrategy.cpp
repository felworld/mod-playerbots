/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MarksmanshipHunterStrategy.h"
#include "Playerbots.h"

MarksmanshipHunterStrategy::MarksmanshipHunterStrategy(PlayerbotAI* botAI) : GenericHunterStrategy(botAI)
{
    // No custom ActionNodeFactory needed
}

// ===== Default Actions =====
std::vector<NextAction> MarksmanshipHunterStrategy::getDefaultActions()
{
    return {
        NextAction("kill command", 5.8f),
        NextAction("kill shot", 5.7f),
        NextAction("serpent sting", 5.6f),
        NextAction("chimera shot", 5.5f),
        NextAction("aimed shot", 5.4f),
        NextAction("arcane shot", 5.3f),
        NextAction("steady shot", 5.2f),
        NextAction("auto shot", 5.1f)
    };
}

// ===== Trigger Initialization ===
void MarksmanshipHunterStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericHunterStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode(
            "silencing shot",
            {
                NextAction("silencing shot", 40.0f)
            }
        )
    );

    // A hunter is the only silence in a lot of groups: shutting the enemy healer up is worth
    // more than another shot on the current target.
    triggers.push_back(
        new TriggerNode(
            "silencing shot on enemy healer",
            {
                NextAction("silencing shot on enemy healer", 40.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "kill command",
            {
                NextAction("kill command", 18.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "target critical health",
            {
                NextAction("kill shot", 18.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "low mana",
            {
                NextAction("viper sting", 17.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "no stings",
            {
                NextAction("serpent sting", 17.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "serpent sting on attacker",
            {
                NextAction("serpent sting on attacker", 16.5f)
            }
        )
    );

    // Instant fillers while moving: the engine refuses Steady Shot (cast time) and Auto Shot
    // (auto-repeat) outright, so without these a moving hunter contributes nothing. Aimed Shot
    // and Chimera Shot are both instant in 3.3.5.
    triggers.push_back(
        new TriggerNode(
            "moving filler",
            {
                NextAction("kill shot", 6.4f),
                NextAction("chimera shot", 6.3f),
                NextAction("aimed shot", 6.2f),
                NextAction("arcane shot", 6.1f),
                NextAction("serpent sting", 6.0f)
            }
        )
    );
}
