/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_GIVEAWAYMGR_H
#define PLAYERBOTS_GIVEAWAYMGR_H

#include "ObjectGuid.h"

#include <map>
#include <mutex>

class Group;
class Item;
class Player;
class Roll;
struct ItemTemplate;

struct PendingGiveaway
{
    ObjectGuid itemGuid;
    ObjectGuid recipientGuid;
    time_t expiresAt = 0;
    bool attempted = false;  // the item was placed in a trade window at least once
};

// When a bot wins a group loot roll on gear it has no use for while another
// member - the real player included - would genuinely upgrade with it, the
// bot (with configurable probability) walks over and trades the item to
// them. Wins are queued here by the roll-reward hook; the "giveaway pending"
// trigger and its action complete the hand-off from the bot's AI loop.
class GiveawayMgr
{
public:
    static GiveawayMgr* instance();

    // Roll-reward hook entry: decide whether this win becomes a giveaway.
    void OnRollRewardItem(Player* winner, Item* item, Roll const* roll);

    bool HasPending(ObjectGuid botGuid);
    bool GetPending(ObjectGuid botGuid, PendingGiveaway& pending);
    void MarkAttempted(ObjectGuid botGuid);
    void Clear(ObjectGuid botGuid);

private:
    static bool GroupHasRealPlayer(Group* group);
    static bool IsUpgradeForRealPlayer(Player* player, ItemTemplate const* proto, int32 randomPropertyId);

    std::mutex _mutex;
    std::map<ObjectGuid, PendingGiveaway> _pending;  // keyed by winner bot guid
};

#define sGiveawayMgr GiveawayMgr::instance()

#endif
