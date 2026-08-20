/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ShadowPriestStrategy.h"
#include "Playerbots.h"
#include "ShadowPriestStrategyActionNodeFactory.h"

ShadowPriestStrategy::ShadowPriestStrategy(PlayerbotAI* botAI) : GenericPriestStrategy(botAI)
{
    actionNodeFactories.Add(new ShadowPriestStrategyActionNodeFactory());
}

std::vector<NextAction> ShadowPriestStrategy::getDefaultActions()
{
    return {
        NextAction("mind blast", ACTION_DEFAULT + 0.3f),
        NextAction("mind flay", ACTION_DEFAULT + 0.2f),
        NextAction("shadow word: death", ACTION_DEFAULT + 0.1f), // cast during movement
        NextAction("shoot", ACTION_DEFAULT)
    };
}

void ShadowPriestStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericPriestStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode(
            "shadowform",
            {
                NextAction("shadowform", ACTION_HIGH)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "low mana",
            {
                NextAction("dispersion", ACTION_HIGH + 5)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "critical health",
            {
                NextAction("dispersion", ACTION_HIGH + 5)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "silence",
            {
                NextAction("silence", ACTION_INTERRUPT + 1)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "silence on enemy healer",
            {
                NextAction("silence on enemy healer", ACTION_INTERRUPT)
            }
        )
    );
    // Psychic Horror disarms the melee that closed on the priest for ten seconds; it is the
    // shadow answer alongside the fear, one tier below it and above simply running.
    triggers.push_back(
        new TriggerNode(
            "psychic scream",
            {
                NextAction("psychic horror", ACTION_MOVE + 9.5f)
            }
        )
    );
    // Instant fillers while moving: Mind Blast has a cast bar and Mind Flay is channelled, so a
    // kiting shadow priest keeps its dots rolling and finishes with Shadow Word: Death instead.
    triggers.push_back(
        new TriggerNode(
            "moving filler",
            {
                NextAction("shadow word: pain", ACTION_DEFAULT + 0.9f),
                NextAction("devouring plague", ACTION_DEFAULT + 0.8f),
                NextAction("shadow word: death", ACTION_DEFAULT + 0.7f)
            }
        )
    );
}

void ShadowPriestAoeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "shadow word: pain on attacker",
            {
                NextAction("shadow word: pain on attacker", ACTION_NORMAL + 5)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "vampiric touch on attacker",
            {
                NextAction("vampiric touch on attacker", ACTION_NORMAL + 4)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "mind sear channel check",
            {
                NextAction("cancel channel", ACTION_HIGH + 5)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "medium aoe",
            {
                NextAction("mind sear", ACTION_HIGH + 4)
            }
        )
    );
}

void ShadowPriestDebuffStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "vampiric touch",
            {
                NextAction("vampiric touch", ACTION_HIGH + 3)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "devouring plague",
            {
                NextAction("devouring plague", ACTION_HIGH + 2)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "shadow word: pain",
            {
                NextAction("shadow word: pain", ACTION_HIGH + 1)
            }
        )
    );
}
