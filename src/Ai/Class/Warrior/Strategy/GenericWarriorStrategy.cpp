/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GenericWarriorStrategy.h"
#include "Playerbots.h"

GenericWarriorStrategy::GenericWarriorStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI)
{
    actionNodeFactories.Add(new WarriorStanceRequirementActionNodeFactory());
}

void GenericWarriorStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    CombatStrategy::InitTriggers(triggers);
    triggers.push_back(new TriggerNode(
        "enemy out of melee", { NextAction("reach melee", ACTION_HIGH + 1) }));
    triggers.push_back(new TriggerNode(
        "fear sleep sap", { NextAction("berserker rage", ACTION_EMERGENCY + 1) }));
    // A paladin bubble, an Ice Block or a Hand of Protection ends the fight unless someone
    // strips it; every spec carries Shattering Throw and it is worth the stance dance.
    triggers.push_back(new TriggerNode(
        "shattering throw trigger", { NextAction("shattering throw", ACTION_INTERRUPT + 1) }));
}

class WarrirorAoeStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    WarrirorAoeStrategyActionNodeFactory()
    {

    }

private:

};

WarrirorAoeStrategy::WarrirorAoeStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI)
{
    actionNodeFactories.Add(new WarrirorAoeStrategyActionNodeFactory());
}

void WarrirorAoeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode(
        "light aoe", { NextAction("sweeping strikes", ACTION_HIGH + 7),
                                       NextAction("bladestorm", ACTION_HIGH + 6),
                                       NextAction("thunder clap", ACTION_HIGH + 5),
                                       NextAction("shockwave", ACTION_HIGH + 4),
                                       NextAction("demoralizing shout without life time check", ACTION_HIGH + 1),
                                       NextAction("cleave", ACTION_HIGH) }));
    triggers.push_back(
        new TriggerNode("shockwave on snare target",
                        { NextAction("shockwave on snare target", ACTION_HIGH + 5) }));

}
