/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DpsRogueStrategy.h"
#include "Playerbots.h"

class DpsRogueStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    DpsRogueStrategyActionNodeFactory()
    {
        creators["sinister strike"] = &sinister_strike;
        creators["kick"] = &kick;
        creators["backstab"] = &backstab;
        creators["rupture"] = &rupture;
    }

private:
    static ActionNode* sinister_strike([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "sinister strike",
            /*P*/ {},
            /*A*/ {
                NextAction("melee") },
            /*C*/ {}
        );
    }
    // With Kick on cooldown the interrupt falls to the stun, then to Gouge - which also buys
    // the bot a few seconds to regain energy.
    static ActionNode* kick([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "kick",
            /*P*/ {},
            /*A*/ {
                NextAction("kidney shot"),
                NextAction("gouge") },
            /*C*/ {}
        );
    }
    static ActionNode* backstab([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "backstab",
            /*P*/ {},
            /*A*/ {
                NextAction("sinister strike") },
            /*C*/ {}
        );
    }
    static ActionNode* rupture([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "rupture",
            /*P*/ {},
            /*A*/ {
                NextAction("eviscerate") },
            /*C*/ {}
        );
    }
};

DpsRogueStrategy::DpsRogueStrategy(PlayerbotAI* botAI) : MeleeCombatStrategy(botAI)
{
    actionNodeFactories.Add(new DpsRogueStrategyActionNodeFactory());
}

std::vector<NextAction> DpsRogueStrategy::getDefaultActions()
{
    return {
        NextAction("killing spree", ACTION_DEFAULT + 0.1f),
        NextAction("melee", ACTION_DEFAULT)
    };
}

void DpsRogueStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    MeleeCombatStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode(
            "high energy available",
            {
                NextAction("garrote", ACTION_HIGH + 7),
                NextAction("ambush", ACTION_HIGH + 6)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "high energy available",
            {
                NextAction("sinister strike", ACTION_NORMAL + 3)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "slice and dice",
            {
                NextAction("slice and dice", ACTION_HIGH + 2)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "combo points 5 available",
            {
                NextAction("rupture", ACTION_HIGH + 1),
                NextAction("eviscerate", ACTION_HIGH)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "target with combo points almost dead",
            {
                NextAction("eviscerate", ACTION_HIGH + 2)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "medium threat",
            {
                NextAction("vanish", ACTION_HIGH)
            }
        )
    );

    // Losing a player fight: Vanish resets it outright, Cloak sheds the magic and the DoTs that
    // would break the stealth, Blind buys the ten seconds to walk away.
    triggers.push_back(
        new TriggerNode(
            "pvp escape",
            {
                NextAction("vanish", ACTION_HIGH),
                NextAction("cloak of shadows", ACTION_HIGH - 1),
                NextAction("blind", ACTION_HIGH - 2)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "low health",
            {
                NextAction("evasion", ACTION_HIGH + 9),
                NextAction("feint", ACTION_HIGH + 8)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "critical health",
            {
                NextAction("cloak of shadows", ACTION_HIGH + 7)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "kick",
            {
                NextAction("kick", ACTION_INTERRUPT + 2)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "kick on enemy healer",
            {
                NextAction("kick on enemy healer", ACTION_INTERRUPT + 1)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "light aoe",
            {
                NextAction("blade flurry", ACTION_HIGH + 3)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "blade flurry",
                {
                NextAction("blade flurry", ACTION_HIGH + 2)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "enemy out of melee",
            {
                NextAction("stealth", ACTION_HIGH + 4),
                NextAction("shadowstep", ACTION_HIGH + 3),
                NextAction("sprint", ACTION_HIGH + 2),
                NextAction("reach melee", ACTION_HIGH + 1)
            }
        )
    );

    // Entering combat already stealthed (duel countdown, ambush) swaps in
    // the stealthed strategy for the opener; its "no stealth" node swaps
    // back once stealth drops. Without this, nothing ever flips forward.
    triggers.push_back(
        new TriggerNode(
            "in stealth",
            {
                NextAction("check stealth", ACTION_EMERGENCY)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "expose armor",
            {
                NextAction("expose armor", ACTION_HIGH + 3)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "low tank threat",
            {
                NextAction("tricks of the trade on main tank", ACTION_HIGH + 7)
            }
        )
    );

    // Kidney Shot is the stun that wins a duel, not a spare Eviscerate: only on a
    // player-controlled target that is not already locked down or diminished.
    triggers.push_back(
        new TriggerNode(
            "kidney shot",
            {
                NextAction("kidney shot", ACTION_HIGH + 4)
            }
        )
    );

    // Ten seconds without weapons ends a warrior's or another rogue's burst.
    triggers.push_back(
        new TriggerNode(
            "dismantle",
            {
                NextAction("dismantle", ACTION_HIGH + 3)
            }
        )
    );

    // The only thing a rogue can throw at someone running away.
    triggers.push_back(
        new TriggerNode(
            "deadly throw on snare target",
            {
                NextAction("deadly throw", ACTION_NORMAL + 4)
            }
        )
    );
}

class StealthedRogueStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    StealthedRogueStrategyActionNodeFactory()
    {
        creators["ambush"] = &ambush;
        creators["sinister strike"] = &sinister_strike;
    }

private:
    static ActionNode* ambush([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "ambush",
            /*P*/ {},
            /*A*/ { NextAction("garrote") },
            /*C*/ {}
        );
    }

    static ActionNode* sinister_strike([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "sinister strike",
            /*P*/ {},
            /*A*/ { NextAction("cheap shot") },
            /*C*/ {}
        );
    }
};

StealthedRogueStrategy::StealthedRogueStrategy(PlayerbotAI* botAI) : Strategy(botAI)
{
    actionNodeFactories.Add(new StealthedRogueStrategyActionNodeFactory());
}

std::vector<NextAction> StealthedRogueStrategy::getDefaultActions()
{
    return {
        NextAction("ambush", ACTION_NORMAL + 4),
        NextAction("backstab", ACTION_NORMAL + 3),
        NextAction("cheap shot", ACTION_NORMAL + 2),
        NextAction("sinister strike", ACTION_NORMAL + 1),
        NextAction("melee", ACTION_NORMAL)
    };
}

void StealthedRogueStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // While the target hasn't noticed the bot, work around behind it for
    // the opener instead of walking straight in; must outrank the assist
    // strategies' straight-line "reach melee" (ACTION_MOVE).
    triggers.push_back(
        new TriggerNode(
            "stealthed approach",
            {
                NextAction("stealth flank", ACTION_MOVE + 8)
            }
        )
    );

    // The Distract trick: a rogue that knows it turns a watching target
    // away and takes the straight line instead of circling. Must outrank
    // "stealth flank" so the turn happens before the footwork; once the
    // target faces away the trigger goes quiet and flank's walk-in branch
    // finishes the approach.
    triggers.push_back(
        new TriggerNode(
            "distract",
            {
                NextAction("distract", ACTION_MOVE + 9)
            }
        )
    );

    // Stealthed, out of combat, and a second enemy player standing next to the mark: Sap them
    // first so the opener lands one-on-one. Lives here rather than in the "cc" strategy because
    // that one only runs in the combat engine; outranks Distract and the flank so the cast
    // happens before the bot closes the last few yards.
    triggers.push_back(
        new TriggerNode(
            "sap opener",
            {
                NextAction("sap opener", ACTION_MOVE + 10)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "combo points 5 available",
            {
                NextAction("eviscerate", ACTION_HIGH)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "kick",
            {
                NextAction("cheap shot", ACTION_INTERRUPT)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "kick on enemy healer",
            {
                NextAction("cheap shot", ACTION_INTERRUPT)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "behind target",
            {
                NextAction("ambush", ACTION_HIGH)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "not behind target",
            {
                NextAction("cheap shot", ACTION_HIGH)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "enemy flagcarrier near",
            {
                NextAction("sprint", ACTION_EMERGENCY + 1)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "unstealth",
            {
                NextAction("unstealth", ACTION_NORMAL)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "no stealth",
            {
                NextAction("check stealth", ACTION_EMERGENCY)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "sprint",
            {
                NextAction("sprint", ACTION_INTERRUPT)
            }
        )
    );
}

void StealthStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "stealth",
            {
                NextAction("stealth", ACTION_INTERRUPT)
            }
        )
    );
}

void RogueAoeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "light aoe",
            {
                NextAction("blade flurry", ACTION_HIGH)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "medium aoe",
            {
                NextAction("fan of knives", ACTION_NORMAL + 5)
            }
        )
    );
}

void RogueBoostStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "adrenaline rush",
            {
                NextAction("adrenaline rush", ACTION_HIGH + 2)
            }
        )
    );
}

void RogueCcStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "sap",
            {
                NextAction("stealth", ACTION_INTERRUPT),
                NextAction("sap", ACTION_INTERRUPT)
            }
        )
    );
}
