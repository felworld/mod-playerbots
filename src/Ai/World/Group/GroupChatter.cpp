/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GroupChatter.h"

#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Random.h"
#include "Timer.h"

namespace
{
// A party of five is the most that can ever answer the same event in party
// chat; the geometric tail practically never reaches it, but a falloff of 100
// would otherwise roll forever.
constexpr uint32 MAX_SPEAKERS = 5;

// How long one event's quota stands. Long enough to cover a batch that
// trickles in (bot logins are spread over several seconds), short enough that
// an unrelated join a minute later gets its own roll.
constexpr uint32 QUOTA_WINDOW_MS = 15 * IN_MILLISECONDS;

constexpr uint32 PRUNE_INTERVAL_MS = 5 * MINUTE * IN_MILLISECONDS;

uint32 RollSpeakers()
{
    if (!roll_chance_i(static_cast<int32>(sPlayerbotAIConfig.groupChatterChance)))
        return 0;

    // Every speaker past the first has to win its own roll, so the count
    // falls off geometrically instead of scaling with the crowd.
    uint32 speakers = 1;
    while (speakers < MAX_SPEAKERS && roll_chance_i(static_cast<int32>(sPlayerbotAIConfig.groupChatterFalloff)))
        ++speakers;

    return speakers;
}
}  // namespace

int32 GroupChatterBoard::ClaimSpeaker(ObjectGuid room, GroupChatterKind kind)
{
    if (room.IsEmpty())
        return -1;

    uint32 const now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(now);

    Quota& quota = _quotas[room][static_cast<uint8>(kind)];
    if (!quota.rolledMs || getMSTimeDiff(quota.rolledMs, now) >= QUOTA_WINDOW_MS)
    {
        quota.speakers = RollSpeakers();
        quota.taken = 0;
        quota.rolledMs = now;
    }

    if (quota.taken >= quota.speakers)
        return -1;

    return static_cast<int32>(quota.taken++);
}

void GroupChatterBoard::Prune(uint32 now)
{
    if (getMSTimeDiff(_lastPruneMs, now) < PRUNE_INTERVAL_MS)
        return;

    _lastPruneMs = now;
    std::erase_if(_quotas,
                  [&](auto const& pair)
                  {
                      for (Quota const& quota : pair.second)
                          if (quota.rolledMs && getMSTimeDiff(quota.rolledMs, now) < QUOTA_WINDOW_MS)
                              return false;

                      return true;
                  });
}

ObjectGuid GroupChatterRoom(PlayerbotAI* botAI)
{
    if (!botAI)
        return ObjectGuid::Empty;

    if (Player* master = botAI->GetMaster())
        return master->GetGUID();

    Player* bot = botAI->GetBot();
    if (!bot)
        return ObjectGuid::Empty;

    if (Group* group = bot->GetGroup())
        return group->GetGUID();

    return ObjectGuid::Empty;
}

uint32 GroupChatterDelayMs(int32 slot)
{
    if (slot < 0)
        return 0;

    uint32 delay = urand(600, 1800);
    for (int32 i = 0; i < slot; ++i)
        delay += urand(1200, 2600);

    return delay;
}
