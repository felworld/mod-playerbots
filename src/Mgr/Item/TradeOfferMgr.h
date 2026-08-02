/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRADEOFFERMGR_H
#define PLAYERBOTS_TRADEOFFERMGR_H

#include "ObjectGuid.h"

#include <map>
#include <mutex>
#include <vector>

class Item;
class Player;
class PlayerbotAI;
struct ItemTemplate;

// Paid class services sold through the same deal machinery as items: a
// mage portal collects the tip in a trade window and then casts; a warlock
// summon runs the ritual first (the customer is far away by definition) and
// collects when they land.
enum class TradeService : uint8
{
    None = 0,
    Portal,
    Summon,
};

// A trade the bot has committed to with a real player: item(s) or a class
// service for gold across a real trade window. One deal per bot at a time.
struct PendingTradeDeal
{
    ObjectGuid counterpartyGuid;
    uint32 itemId = 0;
    uint32 count = 1;       // units of itemId changing hands
    uint32 price = 0;       // copper, for the whole deal
    bool selling = false;   // true: the bot hands over the item(s) and takes the money
    TradeService service = TradeService::None;  // != None: a service deal (itemId is 0)
    uint32 serviceSpellId = 0;  // portal deals: the "Portal: <city>" spell to cast once paid
    time_t expiresAt = 0;
    time_t departAt = 0;    // cross-city deal: earliest "arrival" (simulated ride time); 0 = local
    bool teleported = false; // cross-city deal: the guarded teleport already happened
    bool attempted = false; // goods/gold were placed in a trade window at least once
    bool accepted = false;  // service deal: the bot has hit accept at least once
    uint32 moneyAtAccept = 0;  // service deal: balance when accepting, to tell payment from a cancel
    bool servicePaid = false;  // service deal: the tip landed; only the delivery remains
};

// Deterministic backbone of WTS/WTB trading: bots advertise what they carry
// but don't want and what they want but don't carry, and committed deals are
// fulfilled by walking up to the counterparty and using a real trade window
// (the "trade deal pending" trigger + TradeFulfillAction, modeled on the
// roll-win giveaway flow). Also owns the market anchor that keeps a bot in
// town after it advertises, so it is still around when the bites come.
class TradeOfferMgr
{
public:
    static TradeOfferMgr* instance();

    // Registers a deal and stamps the (long) committed-deal anchor.
    // departAt != 0 marks a cross-city deal: fulfillment holds until then
    // (the simulated ride) and the expiry leaves walking time on top.
    // Fails when the bot already has a pending deal.
    bool AddDeal(Player* bot, Player* counterparty, uint32 itemId, uint32 count, uint32 price, bool selling,
        time_t departAt = 0);

    // Registers a paid class-service deal (the bot's side of the trade
    // window stays empty; only the customer's gold changes hands).
    // timeoutSecs applies when there is no simulated ride (departAt == 0).
    bool AddServiceDeal(Player* bot, Player* counterparty, TradeService service, uint32 serviceSpellId,
        uint32 price, time_t departAt, time_t timeoutSecs);

    bool HasPending(ObjectGuid botGuid);
    bool GetPending(ObjectGuid botGuid, PendingTradeDeal& deal);
    bool HasDealWith(ObjectGuid botGuid, ObjectGuid traderGuid);
    void MarkAttempted(ObjectGuid botGuid);
    void MarkTeleported(ObjectGuid botGuid);
    // Service deals: stamp the balance snapshot taken when the bot accepts,
    // so a completed trade (balance rose by the price) can be told apart
    // from a customer who cancelled the window.
    void MarkAccepted(ObjectGuid botGuid, uint32 moneyAtAccept);
    void MarkServicePaid(ObjectGuid botGuid);
    void Clear(ObjectGuid botGuid);

    // Market anchor: posting an ad (or engaging with a reply) keeps the bot
    // pottering around town for a short while; a committed deal for longer.
    // Renewals only ever extend the anchor, never shorten it.
    void RenewAdAnchor(ObjectGuid botGuid);
    void RenewDealAnchor(ObjectGuid botGuid);
    bool IsAnchored(ObjectGuid botGuid);
    uint32 AnchorSecondsLeft(ObjectGuid botGuid);

private:
    void ExtendAnchor(ObjectGuid botGuid, time_t until);

    std::mutex _mutex;
    std::map<ObjectGuid, PendingTradeDeal> _deals;   // keyed by bot guid
    std::map<ObjectGuid, time_t> _anchorUntil;       // keyed by bot guid
};

#define sTradeOfferMgr TradeOfferMgr::instance()

// Price quotes and item appraisal shared by the trade chat commands and the
// LLM tools that wrap them. Prices lean on the auction house bot's valuation
// when that module is enabled (internally jittered per call, so quotes never
// exactly match AH listings) and fall back to a vendor-price heuristic.
namespace MarketQuote
{
    // Per-unit prices in copper; 0 when the item has no sensible price.
    uint32 Ask(ItemTemplate const* proto);  // what a bot quotes when selling
    uint32 Bid(ItemTemplate const* proto);  // what a bot is willing to pay

    // Deterministic sanity bounds for committing to a deal at `price`.
    bool SaneSellPrice(ItemTemplate const* proto, uint32 count, uint32 price);
    bool SaneBuyPrice(ItemTemplate const* proto, uint32 count, uint32 price);

    // Gold the bot may spend on a WTB purchase without cutting into repair /
    // training / travel reserves.
    uint32 SpendableMoney(PlayerbotAI* botAI);

    struct Appraisal
    {
        bool wants = false;     // the bot would genuinely use the item
        uint32 stock = 0;       // tradeable units the bot carries to spare
        uint32 bidEach = 0;     // per-unit price it would pay (when wants)
        uint32 askEach = 0;     // per-unit price it would ask (when stock > 0)
        std::string reason;     // short human-readable classification
    };

    Appraisal Appraise(PlayerbotAI* botAI, ItemTemplate const* proto);

    struct Sellable
    {
        ItemTemplate const* proto = nullptr;
        uint32 count = 0;       // total tradeable units carried
        uint32 askEach = 0;
    };

    struct Want
    {
        ItemTemplate const* proto = nullptr;
        uint32 bidEach = 0;
    };

    // The bot's WTS stock: tradeable items it carries with no use for beyond
    // selling them.
    std::vector<Sellable> CollectSellables(PlayerbotAI* botAI);

    // The bot's WTB list: profession/spell reagents it has run out of and
    // consumables it carries but is running low on.
    std::vector<Want> CollectWants(PlayerbotAI* botAI);

    // Registers a deal in TradeOfferMgr after deterministic validation
    // (counterparty reachable, direction makes sense for the bot, price
    // within the sanity bounds, gold/stock actually available) and whispers
    // the counterparty a confirmation. On failure sets `error`.
    bool Commit(PlayerbotAI* botAI, Player* counterparty, ItemTemplate const* proto,
        uint32 count, uint32 price, bool selling, std::string& error);

    // The (jittered) tip a bot quotes for a paid class service; 0 when the
    // service is not for sale (config disabled).
    uint32 ServiceTip(TradeService service);

    // Registers a paid class-service deal with a real player and whispers
    // the quote. Portals travel to a far-away customer like item deals do;
    // summons never travel (the ritual is the travel) but refuse a customer
    // already close enough to just walk over. On failure sets `error`.
    bool CommitService(PlayerbotAI* botAI, Player* counterparty, TradeService service,
        uint32 serviceSpellId, std::string const& destination, std::string& error);
}

#endif
