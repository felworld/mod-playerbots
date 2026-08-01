/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TradeFulfillAction.h"

#include "ChatHelper.h"
#include "Event.h"
#include "Map.h"
#include "NewRpgWpvp.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Random.h"
#include "SharedDefines.h"
#include "TradeData.h"
#include "TradeOfferMgr.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float TRADE_FULFILL_MAX_DISTANCE = 300.0f;

uint32 CountInTrade(TradeData* trade, uint32 itemId)
{
    uint32 count = 0;
    for (uint8 slot = 0; slot < TRADE_SLOT_TRADED_COUNT; ++slot)
        if (Item* item = trade->GetItem(TradeSlots(slot)))
            if (item->GetEntry() == itemId)
                count += item->GetCount();
    return count;
}
}

bool TradeFulfillAction::isUseful() { return !bot->IsInCombat(); }

bool TradeFulfillAction::Execute(Event /*event*/)
{
    PendingTradeDeal deal;
    if (!sTradeOfferMgr->GetPending(bot->GetGUID(), deal))
        return false;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(deal.itemId);
    if (!proto)
    {
        sTradeOfferMgr->Clear(bot->GetGUID());
        return false;
    }

    // A closed window after goods/gold were placed means the trade finished
    // or the counterparty backed out - either way this deal is over.
    if (deal.attempted && !bot->GetTrader())
    {
        sTradeOfferMgr->Clear(bot->GetGUID());
        return false;
    }

    Player* counterparty = ObjectAccessor::FindPlayer(deal.counterpartyGuid);
    if (!counterparty || !counterparty->IsInWorld())
    {
        sTradeOfferMgr->Clear(bot->GetGUID());
        return false;
    }

    if (counterparty->GetMapId() != bot->GetMapId() ||
        bot->GetDistance(counterparty) > TRADE_FULFILL_MAX_DISTANCE)
    {
        // Only an untraveled cross-city deal is entitled to be this far out;
        // a local deal (or one that already ported in) doesn't chase a
        // counterparty who wandered off.
        if (!deal.departAt || deal.teleported)
        {
            sTradeOfferMgr->Clear(bot->GetGUID());
            return false;
        }
        return TravelTo(counterparty, deal);
    }

    if (!counterparty->IsAlive())
        return false;  // wait it out; the deal expires if they stay dead

    if (!deal.selling && bot->GetMoney() < deal.price)
    {
        sTradeOfferMgr->Clear(bot->GetGUID());
        return false;
    }

    if (bot->GetDistance(counterparty) > INTERACTION_DISTANCE)
    {
        // A travel deal's port-in point is a couple hundred yards out - use
        // the far mover for the walk-in, it copes with maze-like city paths.
        if (bot->GetDistance(counterparty) > pathFinderDis)
            return MoveFarTo(WorldPosition(counterparty));
        return MoveNear(counterparty, INTERACTION_DISTANCE / 2);
    }

    if (!bot->GetTrader())
    {
        if (counterparty->GetTrader())
            return false;  // busy with someone else; retry until the deal expires

        WorldPacket packet(CMSG_INITIATE_TRADE);
        packet << counterparty->GetGUID();
        bot->GetSession()->HandleInitiateTradeOpcode(packet);
        return true;
    }

    if (bot->GetTrader() != counterparty)
        return false;

    TradeData* trade = bot->GetTradeData();
    if (!trade)
        return false;

    if (deal.selling)
    {
        uint32 placed = CountInTrade(trade, deal.itemId);
        if (placed < deal.count)
        {
            // Smallest tradeable stacks first, matching the plan the deal
            // count was rounded up to.
            std::vector<Item*> candidates = botAI->GetAiObjectContext()
                ->GetValue<std::vector<Item*>>("inventory items", ChatHelper::FormatQItem(deal.itemId))->Get();
            std::sort(candidates.begin(), candidates.end(),
                [](Item const* a, Item const* b) { return a->GetCount() < b->GetCount(); });

            for (Item* item : candidates)
            {
                if (placed >= deal.count)
                    break;
                if (!item->CanBeTraded() || item->IsInTrade())
                    continue;

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
                    break;

                WorldPacket packet(CMSG_SET_TRADE_ITEM, 3);
                packet << (uint8)tradeSlot;
                packet << (uint8)item->GetBagSlot();
                packet << (uint8)item->GetSlot();
                bot->GetSession()->HandleSetTradeItemOpcode(packet);

                if (!trade->HasItem(item->GetGUID()))
                    continue;  // the core refused this stack; try the next

                placed += item->GetCount();
                sTradeOfferMgr->MarkAttempted(bot->GetGUID());
            }

            if (placed < deal.count)
                return false;  // could not cover the deal yet; retry until expiry
        }
    }
    else if (trade->GetMoney() != deal.price)
    {
        WorldPacket packet(CMSG_SET_TRADE_GOLD, 4);
        packet << deal.price;
        bot->GetSession()->HandleSetTradeGoldOpcode(packet);
        sTradeOfferMgr->MarkAttempted(bot->GetGUID());
    }

    // Accept only while the counterparty's side holds up their end; anything
    // else stays unaccepted, and the deal simply expires if it never does.
    TradeData* theirTrade = counterparty->GetTradeData();
    if (!theirTrade)
        return false;

    bool satisfied = deal.selling
        ? theirTrade->GetMoney() >= deal.price
        : CountInTrade(theirTrade, deal.itemId) >= deal.count;

    if (satisfied && !trade->IsAccepted())
    {
        WorldPacket packet;
        uint32 status = TRADE_STATUS_TRADE_ACCEPT;
        packet << status;
        bot->GetSession()->HandleAcceptTradeOpcode(packet);
    }

    return true;
}

bool TradeFulfillAction::TravelTo(Player* counterparty, PendingTradeDeal const& deal)
{
    // Still "riding": the trip is simulated by simply not being there yet.
    if (time(nullptr) < deal.departAt)
        return false;

    if (bot->IsBeingTeleported() || bot->IsInFlight() || bot->GetMap()->Instanceable() ||
        counterparty->GetMap()->Instanceable())
        return false;

    // Never blink where a real player could watch it happen - at either end.
    if (botAI->HasPlayerNearby(150.0f))
        return false;

    // Land a believable walk short of the counterparty (who is themselves a
    // real player, so the ring stays outside the 150yd guard); if the
    // terrain refuses, retry with a fresh bearing next tick.
    WorldLocation hub(counterparty->GetMapId(), counterparty->GetPositionX(), counterparty->GetPositionY(),
        counterparty->GetPositionZ(), counterparty->GetOrientation());
    float bearing = frand(0.0f, 2.0f * float(M_PI));
    WorldPosition landing;
    if (!SampleGroundNear(counterparty->GetMap(), hub, bearing, float(M_PI), 180.0f, 280.0f, 60.0f, landing) &&
        !SampleGroundNear(counterparty->GetMap(), hub, bearing, float(M_PI), 160.0f, 260.0f, 120.0f, landing))
        return false;

    if (RealPlayerNear(landing, 150.0f))
        return false;

    // Reset(true) wipes rpgInfo, dropping any stale old-map payload along
    // with it - exactly what arriving on a fresh map needs.
    bot->GetMotionMaster()->Clear();
    botAI->Reset(true);
    bot->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);

    if (!bot->TeleportTo(landing))
        return false;

    bot->SendMovementFlagUpdate();
    sTradeOfferMgr->MarkTeleported(bot->GetGUID());
    LOG_DEBUG("playerbots", "[TradeDeal] Bot {} ported in near {} to close a deal (map {} {:.1f},{:.1f},{:.1f})",
        bot->GetName(), counterparty->GetName(), landing.GetMapId(), landing.GetPositionX(),
        landing.GetPositionY(), landing.GetPositionZ());
    return true;
}
