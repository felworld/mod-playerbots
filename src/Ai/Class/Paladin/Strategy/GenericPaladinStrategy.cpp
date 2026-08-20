/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GenericPaladinStrategy.h"
#include "GenericPaladinStrategyActionNodeFactory.h"

GenericPaladinStrategy::GenericPaladinStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericPaladinStrategyActionNodeFactory());
}

void GenericPaladinStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    CombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("hammer of justice interrupt",
        { NextAction("hammer of justice", ACTION_INTERRUPT) }));
    triggers.push_back(new TriggerNode("hammer of justice on enemy healer",
        { NextAction("hammer of justice on enemy healer", ACTION_INTERRUPT) }));
    triggers.push_back(new TriggerNode("hammer of justice on snare target",
        { NextAction("hammer of justice on snare target", ACTION_INTERRUPT) }));
    triggers.push_back(new TriggerNode("critical health", { NextAction("divine shield", ACTION_EMERGENCY) }));
    triggers.push_back(new TriggerNode("critical health", { NextAction("lay on hands", ACTION_EMERGENCY + 1) }));
    triggers.push_back(new TriggerNode("party member critical health",
        { NextAction("lay on hands on party", ACTION_EMERGENCY + 2) }));
    triggers.push_back(new TriggerNode("divine shield low health",
        { NextAction("flash of light", ACTION_EMERGENCY + 3), NextAction("holy light", ACTION_EMERGENCY + 2)}));
    triggers.push_back(new TriggerNode("protect party member",
        { NextAction("hand of protection on party", ACTION_EMERGENCY + 3) }));
    triggers.push_back(new TriggerNode("high mana", { NextAction("divine plea", ACTION_HIGH) }));
    triggers.push_back(new TriggerNode("hand of freedom on party",
        { NextAction("hand of freedom on party", ACTION_HIGH + 4) }));
    // A player target that has walked out of melee is kiting: Judgement of Justice caps its run
    // speed, which is the only judgement that answers that.
    triggers.push_back(new TriggerNode("judgement of justice kite",
        { NextAction("judgement of justice", ACTION_HIGH + 3) }));
}

void PaladinCureStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Below critical heals, above medium ones: a dying tank outranks a Cleanse.
    triggers.push_back(new TriggerNode(
        "cleanse cure disease", { NextAction("cleanse disease", ACTION_CRITICAL_HEAL + 3) }));
    triggers.push_back(
        new TriggerNode("cleanse party member cure disease",
                        { NextAction("cleanse disease on party", ACTION_CRITICAL_HEAL + 2) }));
    triggers.push_back(new TriggerNode(
        "cleanse cure poison", { NextAction("cleanse poison", ACTION_CRITICAL_HEAL + 3) }));
    triggers.push_back(
        new TriggerNode("cleanse party member cure poison",
                        { NextAction("cleanse poison on party", ACTION_CRITICAL_HEAL + 2) }));
    triggers.push_back(new TriggerNode(
        "cleanse cure magic", { NextAction("cleanse magic", ACTION_CRITICAL_HEAL + 3) }));
    triggers.push_back(
        new TriggerNode("cleanse party member cure magic",
                        { NextAction("cleanse magic on party", ACTION_CRITICAL_HEAL + 2) }));
}

void PaladinBoostStrategy::InitTriggers(std::vector<TriggerNode*>& /*triggers*/)
{
    // Nothing to add: the paladin's burst cooldowns (Avenging Wrath, Divine Favor, Divine
    // Illumination) are wired in the spec strategies, where the spec knows what they cost.
}

void PaladinCcStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("turn evil", { NextAction("turn evil", ACTION_HIGH + 1) }));
    // Repentance as real crowd control: the shared "cc target" picks the second attacker, or the
    // player the bot is fighting when it is alone and losing, so it works without a raid marker.
    triggers.push_back(
        new TriggerNode("repentance on cc", { NextAction("repentance on cc", ACTION_HIGH + 2) }));
}

void PaladinHealerDpsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("healer should attack",
                        {
                            NextAction("hammer of wrath", ACTION_DEFAULT + 0.6f),
                            NextAction("holy shock", ACTION_DEFAULT + 0.5f),
                            NextAction("shield of righteousness", ACTION_DEFAULT + 0.4f),
                            NextAction("judgement of light", ACTION_DEFAULT + 0.3f),
                            NextAction("consecration", ACTION_DEFAULT + 0.2f),
                            NextAction("exorcism", ACTION_DEFAULT+ 0.1f),
                            }));
}
