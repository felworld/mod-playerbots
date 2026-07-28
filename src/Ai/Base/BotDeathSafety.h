/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_BOTDEATHSAFETY_H
#define _PLAYERBOT_BOTDEATHSAFETY_H

#include "Common.h"

class Player;

namespace BotDeathSafety
{
// How long a dead bot waits out an enemy player camping its body/graveyard before
// resurrecting anyway.
constexpr int64 CAMP_GIVE_UP_SECONDS = 3 * MINUTE;

// How long a dead bot holds a pending soulstone/reincarnation (instead of releasing)
// while an enemy player is nearby.
constexpr int64 SELF_RES_WAIT_SECONDS = 60;

// True when a live, PvP-flagged enemy player is within range. Uses a grid scan rather
// than the bot's own vision so it also works while dead or as a ghost.
bool EnemyPlayerNear(Player* bot, float range = 40.0f);

// Seconds since the bot died. Works both before spirit release (death timer) and
// after (corpse ghost time).
int64 TimeSinceDeath(Player* bot);
}

#endif
