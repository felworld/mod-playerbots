/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_GENERICWARRIORSTRATEGY_H
#define PLAYERBOTS_GENERICWARRIORSTRATEGY_H

#include "Action.h"
#include "CombatStrategy.h"

class PlayerbotAI;

// Stance requirements (3.3.5). The engine only pushes an action node's prerequisites once the
// action itself is castable, and a wrong-stance ability fails that check, so the stance switch
// has to be the node's *alternative*: the bot flips stance on the tick the ability is wanted and
// casts it on the next one, exactly the stance dance a warrior does by hand. Charge is
// deliberately not listed - it is unusable in combat, so its fallback stays each spec's own gap
// closer rather than a stance flip that would fight the spec's own stance trigger.
class WarriorStanceRequirementActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    WarriorStanceRequirementActionNodeFactory()
    {
        // battle only
        creators["mocking blow"] = &mocking_blow;
        creators["overpower"] = &overpower;
        creators["retaliation"] = &retaliation;
        creators["shattering throw"] = &shattering_throw;

        // berserker only
        creators["berserker rage"] = &berserker_rage;
        creators["recklessness"] = &recklessness;
        creators["whirlwind"] = &whirlwind;
        creators["pummel"] = &pummel;
        creators["pummel on enemy healer"] = &pummel_on_enemy_healer;
        creators["intercept"] = &intercept;

        // defensive only
        creators["taunt"] = &taunt;
        creators["revenge"] = &revenge;
        creators["shield block"] = &shield_block;
        creators["disarm"] = &disarm;
        creators["shield wall"] = &shield_wall;
        creators["intervene"] = &intervene;
    }

private:

    static ActionNode* mocking_blow([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "mocking blow",
            /*P*/ {},
            /*A*/ { NextAction("battle stance") },
            /*C*/ {}
        );
    }

    static ActionNode* overpower([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "overpower",
            /*P*/ {},
            /*A*/ { NextAction("battle stance") },
            /*C*/ {}
        );
    }

    static ActionNode* berserker_rage([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "berserker rage",
            /*P*/ {},
            /*A*/ { NextAction("berserker stance") },
            /*C*/ {}
        );
    }

    static ActionNode* recklessness([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "recklessness",
            /*P*/ {},
            /*A*/ { NextAction("berserker stance") },
            /*C*/ {}
        );
    }

    static ActionNode* whirlwind([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "whirlwind",
            /*P*/ {},
            /*A*/ { NextAction("berserker stance") },
            /*C*/ {}
        );
    }

    static ActionNode* pummel([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "pummel",
            /*P*/ {},
            /*A*/ { NextAction("berserker stance") },
            /*C*/ {}
        );
    }

    static ActionNode* pummel_on_enemy_healer([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "pummel on enemy healer",
            /*P*/ {},
            /*A*/ { NextAction("berserker stance") },
            /*C*/ {}
        );
    }

    static ActionNode* intercept([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "intercept",
            /*P*/ {},
            /*A*/ { NextAction("berserker stance"), NextAction("reach melee") },
            /*C*/ {}
        );
    }

    static ActionNode* taunt([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "taunt",
            /*P*/ {},
            /*A*/ { NextAction("defensive stance") },
            /*C*/ {}
        );
    }

    static ActionNode* revenge([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "revenge",
            /*P*/ {},
            /*A*/ { NextAction("defensive stance") },
            /*C*/ {}
        );
    }

    static ActionNode* shield_block([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "shield block",
            /*P*/ {},
            /*A*/ { NextAction("defensive stance") },
            /*C*/ {}
        );
    }

    static ActionNode* disarm([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "disarm",
            /*P*/ {},
            /*A*/ { NextAction("defensive stance") },
            /*C*/ {}
        );
    }

    static ActionNode* shield_wall([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "shield wall",
            /*P*/ {},
            /*A*/ { NextAction("defensive stance") },
            /*C*/ {}
        );
    }

    static ActionNode* intervene([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "intervene",
            /*P*/ {},
            /*A*/ { NextAction("defensive stance") },
            /*C*/ {}
        );
    }

    static ActionNode* retaliation([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "retaliation",
            /*P*/ {},
            /*A*/ { NextAction("battle stance") },
            /*C*/ {}
        );
    }

    static ActionNode* shattering_throw([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "shattering throw",
            /*P*/ {},
            /*A*/ { NextAction("battle stance") },
            /*C*/ {}
        );
    }
};

class GenericWarriorStrategy : public CombatStrategy
{
public:
    GenericWarriorStrategy(PlayerbotAI* botAI);

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "warrior"; }
};

class WarrirorAoeStrategy : public CombatStrategy
{
public:
    WarrirorAoeStrategy(PlayerbotAI* botAI);

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "aoe"; }
};

#endif
