/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GenericRogueNonCombatStrategy.h"
#include "Playerbots.h"

class GenericRogueNonCombatStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericRogueNonCombatStrategyActionNodeFactory()
    {
        creators["use deadly poison on off hand"] = &use_deadly_poison_on_off_hand;
    }

private:
    static ActionNode* use_deadly_poison_on_off_hand([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("use deadly poison on off hand",
                              /*P*/ {},
                              /*A*/ { NextAction("use instant poison on off hand") },
                              /*C*/ {});
    }
};

GenericRogueNonCombatStrategy::GenericRogueNonCombatStrategy(PlayerbotAI* botAI) : NonCombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericRogueNonCombatStrategyActionNodeFactory());
}

void GenericRogueNonCombatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("player has flag",
                                       { NextAction("sprint", ACTION_EMERGENCY + 1) }));
    triggers.push_back(new TriggerNode("enemy flagcarrier near",
                                       { NextAction("sprint", ACTION_EMERGENCY + 2) }));
    triggers.push_back(
        new TriggerNode("main hand weapon no enchant",
                        { NextAction("use instant poison on main hand", 20.0f) }));

    // Against players the off-hand carries Crippling instead: the snare is what keeps a caster
    // or a healer in melee range, and it outweighs the Deadly Poison stack. Outranks the PvE
    // node so a bot poisoning up in a battleground picks it; a bot carrying no Crippling falls
    // through to the node below.
    triggers.push_back(
        new TriggerNode("off hand weapon no enchant pvp",
                        { NextAction("use crippling poison on off hand", 21.0f) }));

    triggers.push_back(
        new TriggerNode("off hand weapon no enchant",
                        { NextAction("use deadly poison on off hand", 19.0f) }));

    // A bubbled enemy beating on the rogue can't be fought back - Vanish out from under the
    // swings (above the shared "immunity standoff" peel at 43, whose retreat then continues
    // stealthed). The action's own gates cover knowing the spell and its cooldown; the trigger
    // stays quiet for a rogue already stealthed (Felworld).
    triggers.push_back(new TriggerNode("immune enemy attacker",
                                       { NextAction("vanish", 46.0f) }));

    triggers.push_back(new TriggerNode("often", { NextAction("unstealth", 30.0f) }));

    // A rogue opens a duel from stealth: the 3s countdown after the accept
    // is the window to restealth (the non-combat engine runs until the duel
    // actually starts). UnstealthAction stays out of the way for the
    // duration of the duel.
    triggers.push_back(new TriggerNode("duel countdown", { NextAction("stealth", ACTION_EMERGENCY) }));
}
