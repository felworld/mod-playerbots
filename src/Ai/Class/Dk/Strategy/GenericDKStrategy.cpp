/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GenericDKStrategy.h"
#include "DKAiObjectContext.h"
#include "Playerbots.h"

class GenericDKStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericDKStrategyActionNodeFactory()
    {
        creators["killing machine"] = &killing_machine;
        creators["anti magic zone"] = &anti_magic_zone;
        creators["death grip"] = &death_grip;
    }

private:
    static ActionNode* death_grip([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("death grip",
                              /*P*/ {},
                              /*A*/ { NextAction("icy touch") },
                              /*C*/ {});
    }

    static ActionNode* killing_machine([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("killing machine",
                              /*P*/ {},
                              /*A*/ { NextAction("improved icy talons") },
                              /*C*/ {});
    }

    static ActionNode* anti_magic_zone([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("anti magic zone",
                              /*P*/ {},
                              /*A*/ { NextAction("anti magic shell") },
                              /*C*/ {});
    }
};

GenericDKStrategy::GenericDKStrategy(PlayerbotAI* botAI) : MeleeCombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericDKStrategyActionNodeFactory());
}

void GenericDKStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    MeleeCombatStrategy::InitTriggers(triggers);

    // Keep the ghoul on the bot's target instead of letting core PetAI pick its own attacker.
    triggers.push_back(
        new TriggerNode("target changed", { NextAction("pet assist", ACTION_NORMAL + 5) }));

    // Interrupts belong in the interrupt tier: at ACTION_HIGH + 1 Mind Freeze sat below Death and
    // Decay, Pestilence and Raise Dead, so a cast went through while the bot kept up its rotation.
    triggers.push_back(
        new TriggerNode("mind freeze", { NextAction("mind freeze", ACTION_INTERRUPT) }));
    triggers.push_back(
        new TriggerNode("mind freeze on enemy healer",
                        { NextAction("mind freeze on enemy healer", ACTION_INTERRUPT) }));
    // Strangulate is the backup interrupt: a 30 yard silence on a two-minute cooldown, so it is
    // spent only when Mind Freeze cannot reach or is down.
    triggers.push_back(
        new TriggerNode("strangulate", { NextAction("strangulate", ACTION_INTERRUPT - 1) }));
    triggers.push_back(
        new TriggerNode("strangulate on enemy healer",
                        { NextAction("strangulate on enemy healer", ACTION_INTERRUPT - 1) }));

    // A caster is throwing something at the bot and Anti-Magic Shell absorbs it.
    triggers.push_back(
        new TriggerNode("anti magic shell", { NextAction("anti magic shell", ACTION_HIGH + 7) }));
    // Icebound Fortitude is the stun break; nothing else the bot could queue works while stunned.
    triggers.push_back(
        new TriggerNode("stunned", { NextAction("icebound fortitude", ACTION_EMERGENCY) }));

    // Snare whatever is running: a fleeing creature, or the hostile player the bot is fighting
    // once it starts moving (the kite trigger has no lifetime gate, so a player at 10% health
    // still gets chained).
    triggers.push_back(
        new TriggerNode("chains of ice", { NextAction("chains of ice", ACTION_HIGH) }));
    triggers.push_back(
        new TriggerNode("chains of ice kite", { NextAction("chains of ice on target", ACTION_HIGH) }));

    // The melee analogue of a caster being kited: a player target that has left melee range gets
    // gripped back, and while Death Grip is on cooldown Death Coil is the only thing a death
    // knight can throw at range. Death Grip sits above "reach melee", the rest below it, so the
    // bot chases and fills the gaps of the chase with damage.
    triggers.push_back(
        new TriggerNode("player target out of melee", { NextAction("death grip", ACTION_HIGH + 2),
                                                        NextAction("death coil", ACTION_HIGH - 1) }));
    triggers.push_back(new TriggerNode(
        "horn of winter", { NextAction("horn of winter", ACTION_NORMAL + 1) }));
    triggers.push_back(new TriggerNode("critical health",
                                       { NextAction("raise dead", ACTION_HIGH + 6),
                                         NextAction("death pact", ACTION_HIGH + 5) }));

    triggers.push_back(
        new TriggerNode("low health", { NextAction("icebound fortitude", ACTION_HIGH + 5),
                                                        NextAction("rune tap", ACTION_HIGH + 4) }));
    triggers.push_back(
        new TriggerNode("medium aoe", { NextAction("death and decay", ACTION_HIGH + 9),
                                                        NextAction("pestilence", ACTION_NORMAL + 4),
                                                        NextAction("blood boil", ACTION_NORMAL + 3) }));
    triggers.push_back(
        new TriggerNode("pestilence glyph", { NextAction("pestilence", ACTION_HIGH + 9) }));
    triggers.push_back(
        new TriggerNode("no rune",
            {
                NextAction("empower rune weapon", ACTION_HIGH + 1)
            }
        )
    );
}
