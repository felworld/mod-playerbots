/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ENGINEERINGTINKERACTIONS_H
#define PLAYERBOTS_ENGINEERINGTINKERACTIONS_H

#include "Action.h"

class Item;
class Player;
class PlayerbotAI;

// Equipped engineering actives: tinker enchants (Nitro Boosts, Hyperspeed Accelerators,
// Hand-Mounted Pyro Rocket) and on-use gear like the classic rocket boots.
namespace EngineeringTinkers
{
    // On-use spell from the item's permanent enchant (tinker) or the item template; 0 if none.
    uint32 UseSpellId(Item* item);

    // Equipped item in the slot whose on-use spell is ready; nullptr otherwise.
    // With requireSpeedBurst, the spell must grant a run-speed aura (rocket boots family).
    Item* UsableEquipped(Player* bot, uint8 equipSlot, bool requireSpeedBurst);
}

// Pop rocket boots / Nitro Boosts: flag runs, chases, and last-ditch escapes.
class UseRocketBootsAction : public Action
{
public:
    UseRocketBootsAction(PlayerbotAI* botAI) : Action(botAI, "rocket boots") {}

    bool Execute(Event event) override;
    bool isPossible() override;
};

// Fire the glove tinker on cooldown, like any self-respecting engineer.
class UseGloveTinkerAction : public Action
{
public:
    UseGloveTinkerAction(PlayerbotAI* botAI) : Action(botAI, "glove tinker") {}

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;
};

#endif
