/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GenericHunterStrategy.h"

class GenericHunterStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericHunterStrategyActionNodeFactory()
    {
        creators["rapid fire"] = &rapid_fire;
        creators["mongoose bite"] = &mongoose_bite;
        creators["raptor strike"] = &raptor_strike;
        creators["explosive trap"] = &explosive_trap;
    }

private:
    static ActionNode* rapid_fire([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("rapid fire",
                              /*P*/ {},
                              /*A*/ { NextAction("readiness") },
                              /*C*/ {});
    }
    static ActionNode* mongoose_bite([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("mongoose bite",
                              /*P*/ {},
                              /*A*/ { NextAction("raptor strike") },
                              /*C*/ {});
    }
    static ActionNode* raptor_strike([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("raptor strike",
                              /*P*/ { NextAction("melee") },
                              /*A*/ {},
                              /*C*/ {});
    }
    static ActionNode* explosive_trap([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("explosive trap",
                              /*P*/ {},
                              /*A*/ { NextAction("immolation trap") },
                              /*C*/ {});
    }
};

GenericHunterStrategy::GenericHunterStrategy(PlayerbotAI* botAI) : RangedCombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericHunterStrategyActionNodeFactory());
}

void GenericHunterStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    RangedCombatStrategy::InitTriggers(triggers);

    // Pet Triggers
    // Keep the pet on the bot's target instead of letting core PetAI pick its own attacker.
    triggers.push_back(new TriggerNode("target changed", { NextAction("pet assist", 15.0f) }));

    // Mark/Ammo/Mana Triggers
    triggers.push_back(new TriggerNode("no ammo", { NextAction("equip upgrades packet action", 30.0f) }));
    triggers.push_back(new TriggerNode("hunter's mark", { NextAction("hunter's mark", 29.5f) }));
    triggers.push_back(new TriggerNode("rapid fire", { NextAction("rapid fire", 29.0f) }));
    triggers.push_back(new TriggerNode("aspect of the viper", { NextAction("aspect of the viper", 28.0f) }));

    // Aggro/Threat/Defensive Triggers
    triggers.push_back(new TriggerNode("has aggro", { NextAction("concussive shot", 20.0f) }));
    triggers.push_back(new TriggerNode("low tank threat", { NextAction("misdirection on main tank", 27.0f) }));
    triggers.push_back(new TriggerNode("low health", { NextAction("deterrence", 35.0f) }));
    triggers.push_back(new TriggerNode("concussive shot on snare target", { NextAction("concussive shot", 20.0f) }));
    triggers.push_back(new TriggerNode("medium threat", { NextAction("feign death", 35.0f) }));
    triggers.push_back(new TriggerNode("pvp escape", { NextAction("feign death", 35.0f) }));
    triggers.push_back(new TriggerNode("hunters pet medium health", { NextAction("mend pet", 22.0f) }));
    triggers.push_back(new TriggerNode("hunters pet low health", { NextAction("mend pet", 21.0f) }));

    // Dispel Triggers
    triggers.push_back(new TriggerNode("tranquilizing shot enrage",
                                       { NextAction("tranquilizing shot", ACTION_DISPEL) }));
    triggers.push_back(new TriggerNode("tranquilizing shot magic",
                                       { NextAction("tranquilizing shot", ACTION_DISPEL) }));

    // Ranged-based Triggers
    triggers.push_back(new TriggerNode("enemy within melee", { NextAction("explosive trap", 37.0f),
                                                               NextAction("mongoose bite", 22.0f),
                                                               NextAction("wing clip", 21.0f) }));

    triggers.push_back(new TriggerNode("enemy too close for auto shot", { NextAction("disengage", 35.0f),
                                                                          NextAction("flee", 34.0f) }));

    // PvP Kiting Triggers
    // A player who has closed to melee is the hunter's worst position, and traps land at the
    // hunter's feet: drop the trap on the ground we are about to leave, buy the global cooldown
    // with Scatter Shot, then break contact. Freezing Trap outranks the Explosive Trap above
    // because taking a player out of the fight beats damaging them, and Frost Trap covers the
    // case where Freezing Trap is on cooldown. Disengage sits just above the "flee" node so the
    // bot leaps out instead of walking, but below Deterrence and Feign Death at 35.
    triggers.push_back(new TriggerNode("player melee on bot", { NextAction("freezing trap at feet", 39.0f),
                                                                NextAction("scatter shot", 38.0f),
                                                                NextAction("frost trap at feet", 34.8f),
                                                                NextAction("disengage", 34.6f) }));

    // Rooted or snared is how a hunter dies; the pet shakes it off for its master.
    triggers.push_back(new TriggerNode("movement impaired", { NextAction("master's call", 36.0f) }));
}

// ===== AoE Strategy, 2/3+ enemies =====
AoEHunterStrategy::AoEHunterStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI) {}

void AoEHunterStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("volley channel check", { NextAction("cancel channel", 23.0f) }));
    triggers.push_back(new TriggerNode("medium aoe", { NextAction("volley", 22.0f) }));
    triggers.push_back(new TriggerNode("light aoe", { NextAction("multi-shot", 21.0f) }));
}

void HunterCcStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("scare beast", { NextAction("scare beast on cc", 23.0f) }));
    triggers.push_back(new TriggerNode("freezing trap", { NextAction("freezing trap", 23.0f) }));
    triggers.push_back(new TriggerNode("wyvern sting", { NextAction("wyvern sting on cc", 23.0f) }));
}

void HunterTrapWeaveStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("immolation trap no cd", { NextAction("reach melee", 23.0f) }));
}
