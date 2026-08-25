/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_GROUPCHATTER_H
#define PLAYERBOTS_GROUPCHATTER_H

#include <array>
#include <mutex>
#include <unordered_map>

#include "ObjectGuid.h"

class PlayerbotAI;

// Join-side and leave-side lines are rolled apart: a party that has spent its
// hellos still has its goodbyes to spend.
enum class GroupChatterKind : uint8
{
    Greeting,
    Farewell,
};

// How many bots speak when something happens that a whole group notices at
// once - an invite batch landing, the player leaving, a camp finishing.
// Every bot answering is the tell that gave this away; exactly one bot
// answering, every time, is just a quieter tell. So the first bot to reach
// the event rolls how many will speak at all - GroupChatterChance that
// anybody does, then GroupChatterFalloff for each speaker after the first -
// and everyone else claims slots against that quota until it runs out.
//
// The falloff makes the speaker count geometric: usually one, sometimes two,
// rarely three, and the same shape whether two bots are in earshot or
// twenty-five.
//
// All state is mutex-guarded: claimants run on map-update threads, and a
// party's members need not share a map.
class GroupChatterBoard
{
public:
    static GroupChatterBoard& instance()
    {
        static GroupChatterBoard instance;
        return instance;
    }

    // Take this bot's place in the room's speaking order for `kind`, rolling
    // the quota if this is the first claim of the event. Returns the
    // zero-based speaking position, or -1 when the quota is spent - or was
    // rolled at zero, because sometimes nobody says anything.
    int32 ClaimSpeaker(ObjectGuid room, GroupChatterKind kind);

private:
    GroupChatterBoard() = default;

    struct Quota
    {
        uint32 speakers{0};  // rolled once, by whoever claims first
        uint32 taken{0};
        uint32 rolledMs{0};
    };

    void Prune(uint32 now);

    std::mutex _mutex;
    // Per room, one quota per GroupChatterKind.
    std::unordered_map<ObjectGuid, std::array<Quota, 2>> _quotas;
    uint32 _lastPruneMs{0};
};

// The room a line is spoken into: the real player it is aimed at when there
// is one, so their whole entourage shares one quota however it happens to be
// grouped at that moment (bots greet on login, before the invite that groups
// them has even landed). Otherwise the group itself, and empty when the bot
// has neither - there is nobody to hear the line anyway.
ObjectGuid GroupChatterRoom(PlayerbotAI* botAI);

// How long the bot in speaking position `slot` waits before saying its line.
// Even the first speaker waits a beat, because answering on the same tick as
// the event is the other half of the tell, and each later speaker lands after
// the one before it - so a second hello reads as an answer rather than an
// echo.
uint32 GroupChatterDelayMs(int32 slot);

#endif
