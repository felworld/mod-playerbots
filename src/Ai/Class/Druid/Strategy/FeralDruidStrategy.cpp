/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "FeralDruidStrategy.h"
#include "Playerbots.h"

class FeralDruidStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    FeralDruidStrategyActionNodeFactory()
    {
        creators["survival instincts"] = &survival_instincts;
        creators["thorns"] = &thorns;
        creators["cure poison"] = &cure_poison;
        creators["cure poison on party"] = &cure_poison_on_party;
        creators["abolish poison"] = &abolish_poison;
        creators["abolish poison on party"] = &abolish_poison_on_party;
    }

private:
    static ActionNode* survival_instincts([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("survival instincts",
                              /*P*/ {},
                              /*A*/ { NextAction("barkskin") },
                              /*C*/ {});
    }

    static ActionNode* thorns([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("thorns",
                              /*P*/ { NextAction("caster form") },
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* cure_poison([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("cure poison",
                              /*P*/ { NextAction("caster form") },
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* cure_poison_on_party([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("cure poison on party",
                              /*P*/ { NextAction("caster form") },
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* abolish_poison([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("abolish poison",
                              /*P*/ { NextAction("caster form") },
                              /*A*/ {},
                              /*C*/ {});
    }

    static ActionNode* abolish_poison_on_party([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("abolish poison on party",
                              /*P*/ { NextAction("caster form") },
                              /*A*/ {},
                              /*C*/ {});
    }

};

FeralDruidStrategy::FeralDruidStrategy(PlayerbotAI* botAI) : GenericDruidStrategy(botAI)
{
    actionNodeFactories.Add(new FeralDruidStrategyActionNodeFactory());
    actionNodeFactories.Add(new ShapeshiftDruidStrategyActionNodeFactory());
}

void FeralDruidStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericDruidStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "enemy out of melee", { NextAction("reach melee", 21.0f) }));
    triggers.push_back(new TriggerNode(
        "low health", { NextAction("survival instincts", 91.0f) }));
    // Feral had no in-combat self-heal at all: nothing ever fired a heal while the bot was in cat
    // or bear form. The heal action nodes shift out through their "caster form" prerequisite and
    // the spec's own form trigger shifts straight back, so the only thing missing was a trigger.
    // 36.0 clears ACTION_MOVE (30) - below it a kiting PvP fight starves the heal. Regrowth reads
    // as not-useful while its own HoT ticks, so the instant Rejuvenation backs it up: the two
    // HoTs stack, and once both are rolling the bot has done what a feral can and keeps fighting
    // in form (Felworld).
    triggers.push_back(new TriggerNode(
        "critical health", { NextAction("regrowth", 36.0f), NextAction("rejuvenation", 35.0f) }));
    // Cats cash a Predator's Swiftness proc in for an instant Healing Touch already at low
    // health: the proc's value is making the slow, big heal instant - Regrowth is fast anyway and
    // covered above, and would read as not-useful whenever its HoT from the critical-health rung
    // is still ticking, wasting the proc. Bears never generate the proc, so the trigger
    // self-gates (Felworld).
    triggers.push_back(new TriggerNode(
        "predator's swiftness and low health", { NextAction("healing touch", 34.0f) }));
    triggers.push_back(new TriggerNode("player has flag",
                                       { NextAction("dash", 92.0f) }));
    triggers.push_back(new TriggerNode("enemy flagcarrier near",
                                       { NextAction("dash", 92.0f) }));
}

void FeralChargeDruidStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    Player* bot = botAI->GetBot();

    if (bot->HasSpell(SPELL_CAT_FORM) && !bot->HasAura(AURA_THICK_HIDE))
        triggers.push_back(new TriggerNode(
            "enemy out of melee", { NextAction("feral charge - cat", 29.0f) }));
    else
        triggers.push_back(new TriggerNode(
            "enemy out of melee", { NextAction("feral charge - bear", 18.0f) }));
}
