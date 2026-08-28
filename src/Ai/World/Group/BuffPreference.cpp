/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BuffPreference.h"

#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Timer.h"

namespace
{
// Nothing here expires on a clock - entries die with the group that owns
// them. Reads drop their own stale entry, and this sweep catches the pairs
// whose group ended while nobody was asking.
constexpr uint32 PRUNE_INTERVAL_MS = 5 * MINUTE * IN_MILLISECONDS;

bool SharesGroup(Player* bot, Player* target)
{
    return bot->GetGroup() && bot->GetGroup() == target->GetGroup();
}
}  // namespace

void BuffPreferenceBoard::Set(ObjectGuid bot, ObjectGuid target, uint32 firstRankSpellId)
{
    if (bot.IsEmpty() || target.IsEmpty())
        return;

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(getMSTime());

    if (firstRankSpellId)
    {
        _preferences[bot][target] = firstRankSpellId;
        return;
    }

    auto botIt = _preferences.find(bot);
    if (botIt == _preferences.end())
        return;

    botIt->second.erase(target);
    if (botIt->second.empty())
        _preferences.erase(botIt);
}

uint32 BuffPreferenceBoard::Get(Player* bot, Player* target)
{
    if (!bot || !target)
        return 0;

    std::lock_guard<std::mutex> lock(_mutex);

    auto botIt = _preferences.find(bot->GetGUID());
    if (botIt == _preferences.end())
        return 0;

    auto it = botIt->second.find(target->GetGUID());
    if (it == botIt->second.end())
        return 0;

    if (SharesGroup(bot, target))
        return it->second;

    botIt->second.erase(it);
    if (botIt->second.empty())
        _preferences.erase(botIt);

    return 0;
}

void BuffPreferenceBoard::Prune(uint32 now)
{
    if (getMSTimeDiff(_lastPruneMs, now) < PRUNE_INTERVAL_MS)
        return;

    _lastPruneMs = now;
    std::erase_if(_preferences,
                  [](auto& pair)
                  {
                      Player* bot = ObjectAccessor::FindConnectedPlayer(pair.first);
                      Group const* group = bot ? bot->GetGroup() : nullptr;
                      if (!group)
                          return true;

                      std::erase_if(pair.second,
                                    [group](auto const& entry) { return !group->IsMember(entry.first); });

                      return pair.second.empty();
                  });
}
