/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GiveawayMgr.h"

#include "Group.h"
#include "Item.h"
#include "ItemUsageValue.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Random.h"
#include "RandomItemMgr.h"
#include "StatsWeightCalculator.h"

#include <algorithm>
#include <limits>

namespace
{
constexpr time_t GIVEAWAY_TIMEOUT_SECS = 120;
}

GiveawayMgr* GiveawayMgr::instance()
{
    static GiveawayMgr instance;
    return &instance;
}

bool GiveawayMgr::GroupHasRealPlayer(Group* group)
{
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsInWorld() && !GET_PLAYERBOT_AI(member))
            return true;
    }

    return false;
}

bool GiveawayMgr::IsUpgradeForRealPlayer(Player* player, ItemTemplate const* proto, int32 randomPropertyId)
{
    if (proto->InventoryType == INVTYPE_NON_EQUIP)
        return false;

    if (player->CanUseItem(proto) != EQUIP_ERR_OK)
        return false;

    if (proto->Class == ITEM_CLASS_WEAPON && !sRandomItemMgr.CanEquipWeapon(proto, player->getClass()))
        return false;

    if (proto->Class == ITEM_CLASS_ARMOR &&
        !sRandomItemMgr.CanEquipArmor(proto, player->getClass(), player->GetLevel()))
        return false;

    StatsWeightCalculator calculator(player);
    calculator.SetItemSetBonus(false);
    calculator.SetOverflowPenalty(false);
    float newScore = calculator.CalculateItem(proto->ItemId, randomPropertyId);
    if (newScore <= 0)
        return false;

    uint8 slot = player->FindEquipSlot(proto, NULL_SLOT, true);
    if (slot == NULL_SLOT)
        return false;

    // Paired slots share the judgment: compare against the weakest holder.
    std::vector<uint8> slots = { slot };
    if (slot == EQUIPMENT_SLOT_FINGER1)
        slots.push_back(EQUIPMENT_SLOT_FINGER2);
    else if (slot == EQUIPMENT_SLOT_TRINKET1)
        slots.push_back(EQUIPMENT_SLOT_TRINKET2);

    float worstOldScore = std::numeric_limits<float>::max();
    for (uint8 checkSlot : slots)
    {
        Item* oldItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, checkSlot);
        if (!oldItem)
            return true;

        float oldScore = calculator.CalculateItem(oldItem->GetEntry(),
            oldItem->GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID));
        worstOldScore = std::min(worstOldScore, oldScore);
    }

    return newScore > worstOldScore * sPlayerbotAIConfig.equipUpgradeThreshold;
}

void GiveawayMgr::OnRollRewardItem(Player* winner, Item* item, Roll const* roll)
{
    if (sPlayerbotAIConfig.rollWinGiveawayChance <= 0.0f)
        return;

    PlayerbotAI* winnerAI = GET_PLAYERBOT_AI(winner);
    if (!winnerAI)
        return;

    Group* group = winner->GetGroup();
    if (!group || group->isBGGroup() || group->isBFGroup())
        return;

    if (!GroupHasRealPlayer(group))
        return;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto || (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR))
        return;

    // Covers the BoP-trade window for soulbound roll wins (trade = true).
    if (!item->CanBeTraded(false, true))
        return;

    int32 randomPropertyId = item->GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID);
    std::string qualifier = std::to_string(proto->ItemId);
    if (randomPropertyId)
        qualifier += "," + std::to_string(randomPropertyId);

    ItemUsage myUsage = winnerAI->GetAiObjectContext()->GetValue<ItemUsage>("item usage", qualifier)->Get();
    if (myUsage != ITEM_USAGE_NONE && myUsage != ITEM_USAGE_VENDOR && myUsage != ITEM_USAGE_AH &&
        myUsage != ITEM_USAGE_DISENCHANT)
        return;

    if (frand(0.0f, 1.0f) >= sPlayerbotAIConfig.rollWinGiveawayChance)
        return;

    // Anyone who was in the roll is a candidate - including members who
    // passed, since polite players pass on things they could use. A real
    // player takes priority over bot recipients.
    Player* recipient = nullptr;
    for (auto const& [voterGuid, vote] : roll->playerVote)
    {
        if (voterGuid == winner->GetGUID())
            continue;

        Player* member = ObjectAccessor::FindPlayer(voterGuid);
        if (!member || !member->IsInWorld() || member->GetGroup() != group)
            continue;

        if (item->IsBindedNotWith(member))
            continue;

        if (PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member))
        {
            ItemUsage usage = memberAI->GetAiObjectContext()->GetValue<ItemUsage>("item usage", qualifier)->Get();
            if (usage != ITEM_USAGE_EQUIP && usage != ITEM_USAGE_REPLACE)
                continue;

            if (!recipient)
                recipient = member;
        }
        else if (IsUpgradeForRealPlayer(member, proto, randomPropertyId))
        {
            recipient = member;
            break;
        }
    }

    if (!recipient)
        return;

    std::lock_guard<std::mutex> lock(_mutex);
    PendingGiveaway& pending = _pending[winner->GetGUID()];
    pending.itemGuid = item->GetGUID();
    pending.recipientGuid = recipient->GetGUID();
    pending.expiresAt = time(nullptr) + GIVEAWAY_TIMEOUT_SECS;
    pending.attempted = false;
}

bool GiveawayMgr::HasPending(ObjectGuid botGuid)
{
    PendingGiveaway pending;
    return GetPending(botGuid, pending);
}

bool GiveawayMgr::GetPending(ObjectGuid botGuid, PendingGiveaway& pending)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _pending.find(botGuid);
    if (it == _pending.end())
        return false;

    if (time(nullptr) > it->second.expiresAt)
    {
        _pending.erase(it);
        return false;
    }

    pending = it->second;
    return true;
}

void GiveawayMgr::MarkAttempted(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _pending.find(botGuid);
    if (it != _pending.end())
        it->second.attempted = true;
}

void GiveawayMgr::Clear(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _pending.erase(botGuid);
}
