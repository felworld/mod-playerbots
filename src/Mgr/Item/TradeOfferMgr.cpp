/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TradeOfferMgr.h"

#include "AuctionHouseBot.h"
#include "BudgetValues.h"
#include "ChatHelper.h"
#include "DBCStores.h"
#include "Item.h"
#include "ItemUsageValue.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "Random.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <algorithm>
#include <set>
#include <unordered_set>

namespace
{
constexpr time_t TRADE_DEAL_TIMEOUT_SECS = 120;

// A paid summon needs room for the group invite (up to a minute), the
// ritual, and the customer accepting and settling up after they land.
constexpr time_t TRADE_SERVICE_SUMMON_TIMEOUT_SECS = 300;

// Cross-city deals: the simulated ride to a far-away counterparty (flat for
// a cross-continent boat/zeppelin hop, distance-paced on the same map) and
// the walking time left after arrival to actually close the window.
constexpr time_t TRADE_TRAVEL_RIDE_MIN_SECS = 10;
constexpr time_t TRADE_TRAVEL_RIDE_MAX_SECS = 240;
constexpr time_t TRADE_TRAVEL_WALK_GRACE_SECS = 300;

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

bool TradeOfferMgr::AddDeal(Player* bot, Player* counterparty, uint32 itemId, uint32 count, uint32 price, bool selling,
    time_t departAt)
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
        deal.expiresAt = departAt ? departAt + TRADE_TRAVEL_WALK_GRACE_SECS
                                  : time(nullptr) + TRADE_DEAL_TIMEOUT_SECS;
        deal.departAt = departAt;
        deal.teleported = false;
        deal.attempted = false;
    }

    ExtendAnchor(bot->GetGUID(), time(nullptr) +
        urand(sPlayerbotAIConfig.tradeDealAnchorMinSeconds,
            std::max(sPlayerbotAIConfig.tradeDealAnchorMinSeconds, sPlayerbotAIConfig.tradeDealAnchorMaxSeconds)));
    return true;
}

bool TradeOfferMgr::AddServiceDeal(Player* bot, Player* counterparty, TradeService service, uint32 serviceSpellId,
    uint32 price, time_t departAt, time_t timeoutSecs)
{
    if (!bot || !counterparty || service == TradeService::None || !price)
        return false;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _deals.find(bot->GetGUID());
        if (it != _deals.end() && time(nullptr) <= it->second.expiresAt)
            return false;

        PendingTradeDeal& deal = _deals[bot->GetGUID()];
        deal = PendingTradeDeal();
        deal.counterpartyGuid = counterparty->GetGUID();
        deal.count = 0;
        deal.price = price;
        deal.selling = true;  // the customer's gold is the bot's side of the accept check
        deal.service = service;
        deal.serviceSpellId = serviceSpellId;
        deal.expiresAt = departAt ? departAt + TRADE_TRAVEL_WALK_GRACE_SECS
                                  : time(nullptr) + timeoutSecs;
        deal.departAt = departAt;
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

void TradeOfferMgr::MarkTeleported(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _deals.find(botGuid);
    if (it != _deals.end())
        it->second.teleported = true;
}

void TradeOfferMgr::MarkAccepted(ObjectGuid botGuid, uint32 moneyAtAccept)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _deals.find(botGuid);
    if (it != _deals.end())
    {
        it->second.attempted = true;
        it->second.accepted = true;
        it->second.moneyAtAccept = moneyAtAccept;
    }
}

void TradeOfferMgr::MarkServicePaid(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _deals.find(botGuid);
    if (it != _deals.end())
        it->second.servicePaid = true;
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
    // Player-to-player trade deals in silver and gold: quotes round down to
    // whole silver, and an item that cannot fetch at least one silver has no
    // market price at all - it gets vendored, not advertised
    // (felworld/mod-llm#20).
    uint32 QuoteSilver(uint32 copper)
    {
        return copper / 100 * 100;
    }

    // Ads skip leveling leftovers: an uncommon-or-worse item far below the
    // bot's own level is vendor fodder nobody around it still wants, while
    // rare and better pieces are the kind players do hawk at any level
    // (felworld/mod-llm#21).
    constexpr uint32 AD_LEVEL_GRACE = 20;

    // Anything an NPC vendor sells for plain gold in unlimited stock has no
    // player-to-player market: anyone can walk up and buy it, so a WTS or
    // WTB line for it (Symbol of Kings, vendor reagents, ...) reads as
    // clueless (felworld/mod-llm#38). Limited-stock vendor rarities and
    // extended-cost (honor/token) items keep their market.
    bool VendorSells(uint32 itemId)
    {
        static std::unordered_set<uint32> const vendorStock = []
        {
            std::unordered_set<uint32> stock;
            for (auto const& [entry, _] : *sObjectMgr->GetCreatureTemplates())
                if (VendorItemData const* vendorItems = sObjectMgr->GetNpcVendorItemList(entry))
                    for (VendorItem const* vendorItem : vendorItems->m_items)
                        if (vendorItem && !vendorItem->maxcount && !vendorItem->ExtendedCost)
                            stock.insert(vendorItem->item);
            return stock;
        }();

        return vendorStock.contains(itemId);
    }

    bool WorthAdvertising(ItemTemplate const* proto, uint8 botLevel)
    {
        if (VendorSells(proto->ItemId))
            return false;

        if (proto->Quality >= ITEM_QUALITY_RARE)
            return true;

        // Equipment and consumables carry a required level; trade goods do
        // not, so their item level stands in for the content tier.
        uint32 band = proto->RequiredLevel ? proto->RequiredLevel : proto->ItemLevel;
        return band + AD_LEVEL_GRACE >= botLevel;
    }

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
        return QuoteSilver(std::max<uint32>(buyout, proto->SellPrice));

    uint32 base = FallbackBase(proto);
    if (!base)
        return 0;

    return QuoteSilver(base * urand(180, 260) / 100);
}

uint32 Bid(ItemTemplate const* proto)
{
    if (!proto)
        return 0;

    uint32 bid = 0;
    uint32 buyout = 0;
    if (AhPrices(proto, bid, buyout))
        return QuoteSilver(std::min(bid, buyout));

    uint32 base = FallbackBase(proto);
    if (!base)
        return 0;

    return QuoteSilver(base * urand(120, 180) / 100);
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
        if (!WorthAdvertising(proto, botAI->GetBot()->GetLevel()))
            continue;

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
        if (VendorSells(proto->ItemId))
            return;

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

namespace
{
    // Within this range a deal closes on foot; a counterparty further out
    // (another city, another continent) is met by simulated travel instead:
    // fulfillment holds for a ride-time estimate, then the bot blinks over
    // unobserved and walks the last stretch in (TradeFulfillAction).
    constexpr float TRADE_DEAL_MAX_DISTANCE = 300.0f;

    // Only whole stacks cross a trade window, so a sell plan rounds the
    // request up to the cheapest covering set: one just-big-enough stack when
    // it exists, otherwise small stacks piled up. Returns the unit total, 0
    // when the bot cannot cover the request.
    uint32 PlanSellCount(PlayerbotAI* botAI, uint32 itemId, uint32 requested)
    {
        std::vector<Item*> items = botAI->GetAiObjectContext()
            ->GetValue<std::vector<Item*>>("inventory items", ChatHelper::FormatQItem(itemId))->Get();

        std::vector<uint32> stacks;
        for (Item* item : items)
            if (item->CanBeTraded())
                stacks.push_back(item->GetCount());

        std::sort(stacks.begin(), stacks.end());

        uint32 singleCover = 0;
        for (uint32 stack : stacks)
            if (stack >= requested)
            {
                singleCover = stack;
                break;
            }

        uint32 pileCover = 0;
        for (uint32 stack : stacks)
        {
            pileCover += stack;
            if (pileCover >= requested)
                break;
        }
        if (pileCover < requested)
            pileCover = 0;

        if (singleCover && (!pileCover || singleCover <= pileCover))
            return singleCover;
        return pileCover;
    }
}

bool Commit(PlayerbotAI* botAI, Player* counterparty, ItemTemplate const* proto,
    uint32 count, uint32 price, bool selling, std::string& error)
{
    Player* bot = botAI->GetBot();

    if (!proto)
    {
        error = "no such item";
        return false;
    }

    if (!counterparty || counterparty == bot || !counterparty->IsInWorld() || !counterparty->IsAlive())
    {
        error = "no such trader here";
        return false;
    }

    // Only real players are worth the hand-off; between bots the haggling
    // itself is the ambience, and a formal deal would tie both up for nothing.
    if (GET_PLAYERBOT_AI(counterparty))
    {
        error = "no need to arrange a hand-off with them - a word between regulars settles it";
        return false;
    }

    if (counterparty->GetTeamId() != bot->GetTeamId())
    {
        error = "cannot trade across factions";
        return false;
    }

    // Local deals close on foot. A counterparty out of walking range means a
    // trip: hold fulfillment for a simulated ride, as long as both ends are
    // out in the open world.
    time_t departAt = 0;
    bool local = counterparty->GetMapId() == bot->GetMapId() &&
        bot->GetDistance(counterparty) <= TRADE_DEAL_MAX_DISTANCE;
    if (!local)
    {
        if (bot->GetMap()->Instanceable() || counterparty->GetMap()->Instanceable())
        {
            error = "they are somewhere you cannot reach";
            return false;
        }

        time_t rideSecs = counterparty->GetMapId() == bot->GetMapId()
            ? time_t(bot->GetDistance(counterparty) / 15.0f)  // ground-mount pace
            : TRADE_TRAVEL_RIDE_MAX_SECS;                     // boat/zeppelin hop plus the ride
        rideSecs = std::clamp(rideSecs, TRADE_TRAVEL_RIDE_MIN_SECS, TRADE_TRAVEL_RIDE_MAX_SECS);
        departAt = time(nullptr) + rideSecs;
    }

    if (!count || !price)
    {
        error = "a deal needs an amount and a price";
        return false;
    }

    Appraisal appraisal = Appraise(botAI, proto);
    if (selling)
    {
        if (appraisal.wants)
        {
            error = "you need that item yourself";
            return false;
        }

        if (!SaneSellPrice(proto, count, price))
        {
            error = "price too low to sell at";
            return false;
        }

        uint32 planned = PlanSellCount(botAI, proto->ItemId, count);
        if (!planned)
        {
            error = "not enough of the item to cover the deal";
            return false;
        }
        count = planned;
    }
    else
    {
        if (!appraisal.wants)
        {
            error = "you have no use for that item";
            return false;
        }

        if (!SaneBuyPrice(proto, count, price))
        {
            error = "price too high to pay";
            return false;
        }

        if (price > SpendableMoney(botAI))
        {
            error = "you cannot spare that much gold";
            return false;
        }
    }

    if (!sTradeOfferMgr->AddDeal(bot, counterparty, proto->ItemId, count, price, selling, departAt))
    {
        error = "you already have a trade in progress";
        return false;
    }

    std::string item = ChatHelper::FormatItem(proto, count > 1 ? count : 0);
    std::string money = ChatHelper::formatMoney(price);
    std::string confirmation;
    if (departAt)
    {
        std::string zone = botAI->GetLocalizedAreaName(GetAreaEntryByAreaID(bot->GetZoneId()));
        confirmation = selling
            ? PlayerbotTextMgr::instance().GetBotTextOrDefault(
                  "trade_deal_sell_travel_confirm",
                  "Deal - %item for %money. I'm over in %zone right now - on my way, give me a few minutes.",
                  {{"%item", item}, {"%money", money}, {"%zone", zone}})
            : PlayerbotTextMgr::instance().GetBotTextOrDefault(
                  "trade_deal_buy_travel_confirm",
                  "Deal - %money for %item. I'm over in %zone right now - on my way, give me a few minutes.",
                  {{"%item", item}, {"%money", money}, {"%zone", zone}});
    }
    else
    {
        confirmation = selling
            ? PlayerbotTextMgr::instance().GetBotTextOrDefault(
                  "trade_deal_sell_confirm", "Deal - %item for %money. Stay put, I'm bringing it over.",
                  {{"%item", item}, {"%money", money}})
            : PlayerbotTextMgr::instance().GetBotTextOrDefault(
                  "trade_deal_buy_confirm", "Deal - %money for %item. Stay put, I'm coming to you.",
                  {{"%item", item}, {"%money", money}});
    }
    bot->Whisper(confirmation, LANG_UNIVERSAL, counterparty);
    return true;
}

uint32 ServiceTip(TradeService service)
{
    uint32 base = service == TradeService::Portal
        ? sPlayerbotAIConfig.classServicePortalTip
        : sPlayerbotAIConfig.classServiceSummonTip;
    if (!base)
        return 0;

    // Whole silver like every other quote, but a configured tip never
    // rounds away to "free".
    return std::max<uint32>(100, QuoteSilver(base * urand(80, 120) / 100));
}

bool CommitService(PlayerbotAI* botAI, Player* counterparty, TradeService service,
    uint32 serviceSpellId, std::string const& destination, std::string& error)
{
    Player* bot = botAI->GetBot();

    if (!counterparty || counterparty == bot || !counterparty->IsInWorld() || !counterparty->IsAlive())
    {
        error = "no such customer here";
        return false;
    }

    // Services are sold to real players only; between bots a favor is free
    // or it is nothing.
    if (GET_PLAYERBOT_AI(counterparty))
    {
        error = "no charging one of your own - regulars sort each other out";
        return false;
    }

    if (counterparty->GetTeamId() != bot->GetTeamId())
    {
        error = "cannot deal across factions";
        return false;
    }

    uint32 price = ServiceTip(service);
    if (!price)
    {
        error = "you do not charge for that";
        return false;
    }

    bool local = counterparty->GetMapId() == bot->GetMapId() &&
        bot->GetDistance(counterparty) <= TRADE_DEAL_MAX_DISTANCE;

    time_t departAt = 0;
    time_t timeoutSecs = TRADE_DEAL_TIMEOUT_SECS;
    if (service == TradeService::Portal)
    {
        // Portals collect the tip face to face first, so a far-away customer
        // means the same simulated trip an item deal takes.
        if (!local)
        {
            if (bot->GetMap()->Instanceable() || counterparty->GetMap()->Instanceable())
            {
                error = "they are somewhere you cannot reach";
                return false;
            }

            time_t rideSecs = counterparty->GetMapId() == bot->GetMapId()
                ? time_t(bot->GetDistance(counterparty) / 15.0f)  // ground-mount pace
                : TRADE_TRAVEL_RIDE_MAX_SECS;                     // boat/zeppelin hop plus the ride
            rideSecs = std::clamp(rideSecs, TRADE_TRAVEL_RIDE_MIN_SECS, TRADE_TRAVEL_RIDE_MAX_SECS);
            departAt = time(nullptr) + rideSecs;
        }
    }
    else
    {
        // Summons reach anywhere - the ritual is the travel - but a customer
        // already in walking range has no business buying one.
        if (local)
        {
            error = "they are close enough to just walk over";
            return false;
        }
        timeoutSecs = TRADE_SERVICE_SUMMON_TIMEOUT_SECS;
    }

    if (!sTradeOfferMgr->AddServiceDeal(bot, counterparty, service, serviceSpellId, price, departAt, timeoutSecs))
    {
        error = "you already have a deal in progress";
        return false;
    }

    std::string money = ChatHelper::formatMoney(price);
    std::string zone = botAI->GetLocalizedAreaName(GetAreaEntryByAreaID(bot->GetZoneId()));
    std::string quote;
    if (service == TradeService::Portal)
    {
        quote = departAt
            ? PlayerbotTextMgr::instance().GetBotTextOrDefault(
                  "trade_service_portal_travel_quote",
                  "A portal to %dest for %money. I'm over in %zone right now - on my way, give me a few minutes.",
                  {{"%dest", destination}, {"%money", money}, {"%zone", zone}})
            : PlayerbotTextMgr::instance().GetBotTextOrDefault(
                  "trade_service_portal_quote",
                  "A portal to %dest - %money and it's yours. Trade me the coin and I'll open it right up.",
                  {{"%dest", destination}, {"%money", money}});
    }
    else
        quote = PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "trade_service_summon_quote",
            "I can pull you here to %zone - %money for the ritual, settle up when you land.",
            {{"%money", money}, {"%zone", zone}});

    bot->Whisper(quote, LANG_UNIVERSAL, counterparty);
    return true;
}
}
