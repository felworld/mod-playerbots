/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ArmsWarriorStrategy.h"

class ArmsWarriorStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    ArmsWarriorStrategyActionNodeFactory()
    {
        creators["charge"] = &charge;
        creators["death wish"] = &death_wish;
        creators["heroic strike"] = &heroic_strike;
    }

private:
    static ActionNode* charge(PlayerbotAI* /*botAI*/)
    {
        return new ActionNode(
            "charge",
            /*P*/ {},
            /*A*/ { NextAction("reach melee") },
            /*C*/ {}
        );
    }

    static ActionNode* death_wish(PlayerbotAI* /*botAI*/)
    {
        return new ActionNode(
            "death wish",
            /*P*/ {},
            /*A*/ { NextAction("bloodrage") },
            /*C*/ {}
        );
    }

    static ActionNode* heroic_strike(PlayerbotAI* /*botAI*/)
    {
        return new ActionNode(
            "heroic strike",
            /*P*/ {},
            /*A*/ { NextAction("melee") },
            /*C*/ {}
        );
    }
};

ArmsWarriorStrategy::ArmsWarriorStrategy(PlayerbotAI* botAI) : GenericWarriorStrategy(botAI)
{
    actionNodeFactories.Add(new ArmsWarriorStrategyActionNodeFactory());
}

std::vector<NextAction> ArmsWarriorStrategy::getDefaultActions()
{
    return {
        NextAction("bladestorm", ACTION_DEFAULT + 0.2f),
        NextAction("mortal strike", ACTION_DEFAULT + 0.1f),
        NextAction("sunder armor", ACTION_DEFAULT + 0.05f),
        NextAction("melee", ACTION_DEFAULT)
    };
}
void ArmsWarriorStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericWarriorStrategy::InitTriggers(triggers);

    // Heroic Throw opens (or finishes a runner) from 30 yards; Charge covers the out-of-combat
    // gap, Intercept the in-combat one.
    triggers.push_back(
        new TriggerNode(
            "enemy out of melee",
            {
                NextAction("heroic throw", ACTION_MOVE + 11),
                NextAction("charge", ACTION_MOVE + 10)
            }
        )
    );

    // Arms lives in Battle Stance, so its only in-combat gap closer is a stance dance into
    // Intercept - gated on the cooldown and on having the rage for it, and ranked above the
    // Charge chain so the run-in fallback does not swallow it.
    triggers.push_back(
        new TriggerNode(
            "intercept and rage",
            {
                NextAction("intercept", ACTION_MOVE + 12)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "battle stance",
            {
                NextAction("battle stance", ACTION_HIGH + 10)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "battle shout",
            {
                NextAction("battle shout", ACTION_HIGH + 9)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "rend",
            {
                NextAction("rend", ACTION_HIGH + 8)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "rend on attacker",
            {
                NextAction("rend on attacker", ACTION_HIGH + 8)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "mortal strike",
            {
                NextAction("mortal strike", ACTION_HIGH + 3)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "target critical health",
            {
                NextAction("execute", ACTION_HIGH + 5)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "sudden death",
            {
                NextAction("execute", ACTION_HIGH + 5)
            }
        )
    );

    // Hamstring whoever we are actually fighting the moment they try to walk away - the core
    // warrior PvP habit. "snare target" prefers the current target and now sees players.
    triggers.push_back(
        new TriggerNode(
            "hamstring",
            {
                NextAction("hamstring", ACTION_HIGH)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "overpower",
            {
                NextAction("overpower", ACTION_HIGH + 4)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "taste for blood",
            {
                NextAction("overpower", ACTION_HIGH + 4)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "victory rush",
            {
                NextAction("victory rush", ACTION_INTERRUPT)
            }
        )
    );

    // Arms had no interrupt at all. Pummel is Berserker Stance only, so both nodes are gated on
    // the Pummel cooldown - otherwise the bot would dance out of Battle Stance for nothing every
    // time an enemy started casting.
    triggers.push_back(
        new TriggerNode(
            "pummel and can cast",
            {
                NextAction("pummel", ACTION_INTERRUPT)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "pummel on enemy healer and can cast",
            {
                NextAction("pummel on enemy healer", ACTION_INTERRUPT)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "high rage available",
            {
                NextAction("heroic strike", ACTION_HIGH),
                NextAction("slam", ACTION_HIGH + 1)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "bloodrage",
            {
                NextAction("bloodrage", ACTION_HIGH + 2)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "death wish",
            {
                NextAction("death wish", ACTION_HIGH + 2)
            }
        )
    );

    // Intimidating Shout is a cone fear: as a PvE opener it drags whole packs around, so it is
    // reserved for a PvP fight the bot is losing (two enemy players on it, or one while hurt).
    triggers.push_back(
        new TriggerNode(
            "pvp escape",
            {
                NextAction("intimidating shout", ACTION_EMERGENCY)
            }
        )
    );

    // A three-minute heal over time belongs at low health, not at the first scratch.
    triggers.push_back(
        new TriggerNode(
            "low health",
            {
                NextAction("enraged regeneration", ACTION_EMERGENCY)
            }
        )
    );

    // Retaliation answers a melee train, not a health threshold; the action itself requires two
    // melee attackers actually swinging at the bot.
    triggers.push_back(
        new TriggerNode(
            "being attacked",
            {
                NextAction("retaliation", ACTION_HIGH + 5)
            }
        )
    );
}
