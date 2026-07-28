/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BotDeathSafety.h"

#include "CellImpl.h"
#include "Corpse.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"

namespace BotDeathSafety
{
bool EnemyPlayerNear(Player* bot, float range)
{
    std::list<Player*> players;
    Acore::AnyPlayerInObjectRangeCheck check(bot, range, /*reqAlive*/ true, /*disallowGM*/ true);
    Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(bot, players, check);
    Cell::VisitObjects(bot, searcher, range);

    for (Player* enemy : players)
        if (enemy->IsHostileTo(bot) && (enemy->IsPvP() || enemy->IsFFAPvP()))
            return true;

    return false;
}

int64 TimeSinceDeath(Player* bot)
{
    if (Corpse* corpse = bot->GetCorpse())
        return time(nullptr) - corpse->GetGhostTime();

    // Dead but not yet released: the 6 minute release timer counts down from death.
    if (!bot->IsAlive())
        return int64(6 * MINUTE) - int64(bot->GetDeathTimer() / IN_MILLISECONDS);

    return 0;
}
}
