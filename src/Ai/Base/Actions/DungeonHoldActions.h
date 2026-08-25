/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DUNGEONHOLDACTIONS_H
#define PLAYERBOTS_DUNGEONHOLDACTIONS_H

#include "AttackAction.h"

class PlayerbotAI;

// Starts the swing the dungeon hold kept back (Felworld). AttackAction::Attack is the only path to
// Player::Attack, and the strategies only walk it when the target changes - which happened long
// before the main tank picked the mob up. This one takes the target the bot already has, so the
// release does not depend on "dps target" still resolving to it.
class DungeonHoldAttackAction : public AttackAction
{
public:
    DungeonHoldAttackAction(PlayerbotAI* botAI) : AttackAction(botAI, "dungeon hold attack") {}

    std::string const GetTargetName() override { return "current target"; }
};

#endif
