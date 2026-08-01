/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TradeOfferMgr.h"

#include "AuctionHouseBot.h"
#include "BudgetValues.h"
#include "ChatHelper.h"
#include "Item.h"
#include "ItemUsageValue.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Random.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <algorithm>
#include <set>

namespace
{
constexpr time_t TRADE_DEAL_TIMEOUT_SECS = 120;

// Deterministic guardrails around committed prices: however the negotiation
// went, a bot never sells for less than half its own quote (or below vendor
// price) and never pays more than double it.
constexpr uint32 SELL_FLOOR_PERCENT = 50;
constexpr uint32 BUY_CAP_PERCENT = 200;
}

TradeOfferMgr* TradeOfferMgr::instance()
{
    static TradeOfferMgr instance;
    return &instance;
}

bool TradeOfferMgr::AddDeal(Player* bot, Player* counterparty, uint32 itemId, uint32 count, uint32 price, bool selling)
{
    if (!bot || !counterparty || !itemId || !count)
        return false;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _deals.find(bot->GetGUID());
        if (it != _deals.end() && time(nullptr) <= it->second.expiresAt)
            return false;

        PendingTradeDeal& deal = _deals[bot->GetGUID()];
        deal.counterpartyGuid = counterparty->GetGUID();
        deal.itemId = itemId;
        deal.count = count;
        deal.price = price;
        deal.selling = selling;
        deal.expiresAt = time(nullptr) + TRADE_DEAL_TIMEOUT_SECS;
        deal.attempted = false;
    }

    ExtendAnchor(bot->GetGUID(), time(nullptr) +
        urand(sPlayerbotAIConfig.tradeDealAnchorMinSeconds,
            std::max(sPlayerbotAIConfig.tradeDealAnchorMinSeconds, sPlayerbotAIConfig.tradeDealAnchorMaxSeconds)));
    return true;
}

bool TradeOfferMgr::HasPending(ObjectGuid botGuid)
{
    PendingTradeDeal deal;
    return GetPending(botGuid, deal);
}

bool TradeOfferMgr::GetPending(ObjectGuid botGuid, PendingTradeDeal& deal)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _deals.find(botGuid);
    if (it == _deals.end())
        return false;

    if (time(nullptr) > it->second.expiresAt)
    {
        _deals.erase(it);
        return false;
    }

    deal = it->second;
    return true;
}

bool TradeOfferMgr::HasDealWith(ObjectGuid botGuid, ObjectGuid traderGuid)
{
    PendingTradeDeal deal;
    return GetPending(botGuid, deal) && deal.counterpartyGuid == traderGuid;
}

void TradeOfferMgr::MarkAttempted(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _deals.find(botGuid);
    if (it != _deals.end())
        it->second.attempted = true;
}

void TradeOfferMgr::Clear(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _deals.erase(botGuid);
}

void TradeOfferMgr::RenewAdAnchor(ObjectGuid botGuid)
{
    ExtendAnchor(botGuid, time(nullptr) + sPlayerbotAIConfig.tradeAdAnchorSeconds);
}

void TradeOfferMgr::RenewDealAnchor(ObjectGuid botGuid)
{
    ExtendAnchor(botGuid, time(nullptr) +
        urand(sPlayerbotAIConfig.tradeDealAnchorMinSeconds,
            std::max(sPlayerbotAIConfig.tradeDealAnchorMinSeconds, sPlayerbotAIConfig.tradeDealAnchorMaxSeconds)));
}

bool TradeOfferMgr::IsAnchored(ObjectGuid botGuid)
{
    return AnchorSecondsLeft(botGuid) > 0;
}

uint32 TradeOfferMgr::AnchorSecondsLeft(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _anchorUntil.find(botGuid);
    if (it == _anchorUntil.end())
        return 0;

    time_t now = time(nullptr);
    if (now >= it->second)
    {
        _anchorUntil.erase(it);
        return 0;
    }
    return uint32(it->second - now);
}

void TradeOfferMgr::ExtendAnchor(ObjectGuid botGuid, time_t until)
{
    std::lock_guard<std::mutex> lock(_mutex);
    time_t& current = _anchorUntil[botGuid];
    current = std::max(current, until);
}

namespace MarketQuote
{
namespace
{
    // Vendor-price heuristic used when the AH bot's valuation tables are not
    // available (module disabled): quotes above what a vendor pays, jittered
    // so two bots never ask exactly the same.
    uint32 FallbackBase(ItemTemplate const* proto)
    {
        if (proto->SellPrice)
            return proto->SellPrice;
        if (proto->BuyPrice)
            return std::max<uint32>(1, proto->BuyPrice / 4);
        return 0;
    }

    bool AhPrices(ItemTemplate const* proto, uint32& bid, uint32& buyout)
    {
        if (!auctionbot->IsModuleEnabled())
            return false;

        uint64 bid64 = 0;
        uint64 buyout64 = 0;
        auctionbot->CalculateItemValue(proto, bid64, buyout64);
        if (!buyout64)
            return false;

        bid = uint32(std::min<uint64>(bid64, 0xFFFFFFFF));
        buyout = uint32(std::min<uint64>(buyout64, 0xFFFFFFFF));
        return true;
    }
}

uint32 Ask(ItemTemplate const* proto)
{
    if (!proto)
        return 0;

    uint32 bid = 0;
    uint32 buyout = 0;
    if (AhPrices(proto, bid, buyout))
        return std::max<uint32>(buyout, std::max<uint32>(1, proto->SellPrice));

    uint32 base = FallbackBase(proto);
    if (!base)
        return 0;

    return std::max<uint32>(1, base * urand(180, 260) / 100);
}

uint32 Bid(ItemTemplate const* proto)
{
    if (!proto)
        return 0;

    uint32 bid = 0;
    uint32 buyout = 0;
    if (AhPrices(proto, bid, buyout))
        return std::max<uint32>(1, std::min(bid, buyout));

    uint32 base = FallbackBase(proto);
    if (!base)
        return 0;

    return std::max<uint32>(1, base * urand(120, 180) / 100);
}

bool SaneSellPrice(ItemTemplate const* proto, uint32 count, uint32 price)
{
    if (!proto || !count || !price)
        return false;

    uint32 floorEach = std::max(proto->SellPrice, Ask(proto) * SELL_FLOOR_PERCENT / 100);
    return price >= floorEach * count;
}

bool SaneBuyPrice(ItemTemplate const* proto, uint32 count, uint32 price)
{
    if (!proto || !count || !price)
        return false;

    uint32 capEach = std::max<uint32>(1, Bid(proto)) * BUY_CAP_PERCENT / 100;
    return price <= capEach * count;
}

uint32 SpendableMoney(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    uint32 freeMoney = botAI->GetAiObjectContext()
        ->GetValue<uint32>("free money for", std::to_string(uint32(NeedMoneyFor::anything)))->Get();

    // Gold-cheat bots report a fat fake budget, but a trade window drains the
    // real balance - never promise more than the bot actually carries.
    return std::min(freeMoney, bot->GetMoney());
}

Appraisal Appraise(PlayerbotAI* botAI, ItemTemplate const* proto)
{
    Appraisal appraisal;
    if (!proto)
        return appraisal;

    ItemUsage usage = botAI->GetAiObjectContext()
        ->GetValue<ItemUsage>("item usage", std::to_string(proto->ItemId))->Get();

    switch (usage)
    {
        case ITEM_USAGE_EQUIP:
        case ITEM_USAGE_REPLACE:
            appraisal.wants = true;
            appraisal.reason = "an upgrade I could equip";
            break;
        case ITEM_USAGE_SKILL:
            appraisal.wants = true;
            appraisal.reason = "a reagent I am out of";
            break;
        case ITEM_USAGE_USE:
            appraisal.wants = true;
            appraisal.reason = "a consumable I am running low on";
            break;
        case ITEM_USAGE_AMMO:
            appraisal.wants = true;
            appraisal.reason = "ammo I can shoot";
            break;
        case ITEM_USAGE_KEEP:
        case ITEM_USAGE_QUEST:
        case ITEM_USAGE_GUILD_TASK:
            appraisal.reason = "something I already have enough of";
            break;
        default:
            appraisal.reason = "of no use to me";
            break;
    }

    if (appraisal.wants)
        appraisal.bidEach = Bid(proto);
    else
    {
        // Count tradeable copies the bot could part with.
        uint32 stock = 0;
        std::vector<Item*> items = botAI->GetAiObjectContext()
            ->GetValue<std::vector<Item*>>("inventory items", ChatHelper::FormatQItem(proto->ItemId))->Get();
        for (Item* item : items)
            if (item->CanBeTraded())
                stock += item->GetCount();

        appraisal.stock = stock;
        if (stock)
            appraisal.askEach = Ask(proto);
    }

    return appraisal;
}

std::vector<Sellable> CollectSellables(PlayerbotAI* botAI)
{
    std::vector<Sellable> sellables;

    std::vector<Item*> items = botAI->GetAiObjectContext()
        ->GetValue<std::vector<Item*>>("inventory items",
            "usage " + std::to_string(uint32(ITEM_USAGE_AH)))->Get();

    for (Item* item : items)
    {
        if (!item->CanBeTraded())
            continue;

        ItemTemplate const* proto = item->GetTemplate();
        auto it = std::find_if(sellables.begin(), sellables.end(),
            [proto](Sellable const& s) { return s.proto == proto; });
        if (it != sellables.end())
        {
            it->count += item->GetCount();
            continue;
        }

        uint32 ask = Ask(proto);
        if (!ask)
            continue;

        sellables.push_back({ proto, item->GetCount(), ask });
    }

    return sellables;
}

std::vector<Want> CollectWants(PlayerbotAI* botAI)
{
    std::vector<Want> wants;
    Player* bot = botAI->GetBot();

    auto usageOf = [botAI](uint32 itemId)
    {
        return botAI->GetAiObjectContext()
            ->GetValue<ItemUsage>("item usage", std::to_string(itemId))->Get();
    };

    auto addWant = [&wants](ItemTemplate const* proto)
    {
        if (std::find_if(wants.begin(), wants.end(),
                [proto](Want const& w) { return w.proto == proto; }) != wants.end())
            return;

        uint32 bid = Bid(proto);
        if (bid)
            wants.push_back({ proto, bid });
    };

    // Reagents for spells the bot knows that it has run clean out of
    // ("item usage" answers SKILL exactly when the stock is empty and the
    // reagent still matters to a useful spell).
    std::set<uint32> reagentIds;
    for (auto const& [spellId, playerSpell] : bot->GetSpellMap())
    {
        if (!playerSpell || playerSpell->State == PLAYERSPELL_REMOVED || !playerSpell->Active)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        for (int32 reagent : spellInfo->Reagent)
            if (reagent > 0)
                reagentIds.insert(uint32(reagent));
    }

    for (uint32 itemId : reagentIds)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            continue;
        if (usageOf(itemId) == ITEM_USAGE_SKILL)
            addWant(proto);
    }

    // Consumables the bot carries but is running low on ("item usage"
    // answers USE while the stack target is not met).
    std::vector<Item*> consumables = botAI->GetAiObjectContext()
        ->GetValue<std::vector<Item*>>("inventory items",
            "usage " + std::to_string(uint32(ITEM_USAGE_USE)))->Get();
    for (Item* item : consumables)
    {
        ItemTemplate const* proto = item->GetTemplate();
        if (proto->Class != ITEM_CLASS_CONSUMABLE)
            continue;
        addWant(proto);
    }

    return wants;
}
}
