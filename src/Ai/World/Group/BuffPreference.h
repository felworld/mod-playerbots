/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BUFFPREFERENCE_H
#define PLAYERBOTS_BUFFPREFERENCE_H

#include <mutex>
#include <unordered_map>

#include "ObjectGuid.h"

class Player;

// Which buff a particular player wants from a particular bot, for the cases
// where the bot's own upkeep would otherwise choose for them - a paladin
// handing out Kings by role when the healer asked for Wisdom.
//
// Honouring the request once and forgetting it is the whole problem: the
// upkeep loop re-blesses on its own schedule and paves over the choice a
// minute later. So the choice stands for as long as the party the two share -
// long enough for a dungeon run, gone by the next group, never persisted.
//
// Written from the world thread (the `prefer buff` chat command, or mod-llm's
// buff_player tool) and read from map-update threads (bot upkeep), so the
// board is mutex-guarded like GroupChatterBoard.
class BuffPreferenceBoard
{
public:
    static BuffPreferenceBoard& instance()
    {
        static BuffPreferenceBoard instance;
        return instance;
    }

    // Remember that `target` wants `firstRankSpellId` from `bot`. The id is
    // the first rank of the chain - the caster still picks the best rank it
    // knows. Pass 0 to forget.
    void Set(ObjectGuid bot, ObjectGuid target, uint32 firstRankSpellId);

    // The buff `target` asked `bot` for, or 0 when there is none. Party
    // lifetime is enforced here: an entry whose group has ended is dropped
    // the first time anyone asks about it.
    uint32 Get(Player* bot, Player* target);

private:
    BuffPreferenceBoard() = default;

    void Prune(uint32 now);

    std::mutex _mutex;
    // Per bot, the buff each player who asked wants kept on them.
    std::unordered_map<ObjectGuid, std::unordered_map<ObjectGuid, uint32>> _preferences;
    uint32 _lastPruneMs{0};
};

#endif
