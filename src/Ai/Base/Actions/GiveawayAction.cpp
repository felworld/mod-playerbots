/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GiveawayAction.h"

#include "ChatHelper.h"
#include "Event.h"
#include "GiveawayMgr.h"
#include "ObjectAccessor.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "TradeData.h"

bool GiveawayAction::isUseful() { return !bot->IsInCombat(); }

bool GiveawayAction::Execute(Event /*event*/)
{
    PendingGiveaway pending;
    if (!sGiveawayMgr->GetPending(bot->GetGUID(), pending))
        return false;

    Item* item = bot->GetItemByGuid(pending.itemGuid);
    if (!item)
    {
        // Traded away (success) or otherwise gone - either way we're done.
        sGiveawayMgr->Clear(bot->GetGUID());
        return false;
    }

    Player* recipient = ObjectAccessor::FindPlayer(pending.recipientGuid);
    if (!recipient || !recipient->IsInWorld() || recipient->GetGroup() != bot->GetGroup() ||
        recipient->GetMapId() != bot->GetMapId() || !recipient->IsAlive() ||
        !item->CanBeTraded(false, true) || item->IsBindedNotWith(recipient))
    {
        sGiveawayMgr->Clear(bot->GetGUID());
        return false;
    }

    // The recipient closed or declined an offered trade - let it go.
    if (pending.attempted && !bot->GetTrader())
    {
        sGiveawayMgr->Clear(bot->GetGUID());
        return false;
    }

    if (bot->GetDistance(recipient) > INTERACTION_DISTANCE)
        return MoveNear(recipient, INTERACTION_DISTANCE / 2);

    if (!bot->GetTrader())
    {
        if (recipient->GetTrader())
            return false;  // busy with someone else; retry until the offer expires

        WorldPacket packet(CMSG_INITIATE_TRADE);
        packet << recipient->GetGUID();
        bot->GetSession()->HandleInitiateTradeOpcode(packet);
        return true;
    }

    if (bot->GetTrader() != recipient)
        return false;

    TradeData* trade = bot->GetTradeData();
    if (!trade)
        return false;

    if (!trade->HasItem(item->GetGUID()))
    {
        int8 tradeSlot = -1;
        for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
        {
            if (!trade->GetItem(TradeSlots(i)))
            {
                tradeSlot = i;
                break;
            }
        }

        if (tradeSlot == -1)
            return false;

        WorldPacket packet(CMSG_SET_TRADE_ITEM, 3);
        packet << (uint8)tradeSlot;
        packet << (uint8)item->GetBagSlot();
        packet << (uint8)item->GetSlot();
        bot->GetSession()->HandleSetTradeItemOpcode(packet);

        if (!trade->HasItem(item->GetGUID()))
            return false;  // the core refused the item; keep the offer pending

        sGiveawayMgr->MarkAttempted(bot->GetGUID());

        bot->Say(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                     "rpg_item_better_for_player", "You can use this %item better than me, %player.",
                     {{"%item", chat->FormatItem(item->GetTemplate())}, {"%player", recipient->GetName()}}),
                 bot->GetTeamId() == TEAM_ALLIANCE ? LANG_COMMON : LANG_ORCISH);
    }

    // Accept our side (again if the recipient fiddled with the window);
    // the trade completes when the recipient accepts too.
    if (!trade->IsAccepted())
    {
        WorldPacket packet;
        uint32 status = TRADE_STATUS_TRADE_ACCEPT;
        packet << status;
        bot->GetSession()->HandleAcceptTradeOpcode(packet);
    }

    return true;
}
