/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_THROWEXPLOSIVESACTION_H
#define PLAYERBOTS_THROWEXPLOSIVESACTION_H

#include "Action.h"

#include <functional>

class Item;
class Player;
class PlayerbotAI;
struct ItemTemplate;
class Unit;

class ThrowExplosivesAction : public Action
{
public:
    ThrowExplosivesAction(PlayerbotAI* botAI, std::string const name = "throw explosives")
        : Action(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;

    // Item classification, shared with the triggers and PlayerbotFactory stocking.
    static bool IsThrownExplosive(ItemTemplate const* proto);
    static bool IsStunExplosive(ItemTemplate const* proto);
    static bool IsSapperCharge(ItemTemplate const* proto);

    static void VisitExplosives(Player* bot, std::function<void(Item*)> const& visit);

    // Highest-tier usable, off-cooldown explosive the bot carries; nullptr if none.
    static Item* FindBestThrown(Player* bot, bool requireStun = false);
    static Item* FindBestSapper(Player* bot);

    // True when the target is within the throw spell's range and line of sight.
    static bool CanThrowAt(Player* bot, Item* item, Unit* target);

protected:
    virtual Item* PickExplosive();
    bool ThrowAt(Item* item, Unit* target);
};

class GrenadeInterruptAction : public ThrowExplosivesAction
{
public:
    GrenadeInterruptAction(PlayerbotAI* botAI) : ThrowExplosivesAction(botAI, "grenade interrupt") {}

protected:
    Item* PickExplosive() override;
};

class SapperChargeAction : public Action
{
public:
    SapperChargeAction(PlayerbotAI* botAI) : Action(botAI, "sapper charge") {}

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;
};

#endif
