/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRADEDEALACTIONS_H
#define PLAYERBOTS_TRADEDEALACTIONS_H

#include "InventoryAction.h"

class PlayerbotAI;
struct ItemTemplate;

// Shared parsing for the market commands: an item link plus an optional bare
// count and an optional ##g##s##c price anywhere in the text.
namespace TradeDealParse
{
    struct Request
    {
        ItemTemplate const* proto = nullptr;
        uint32 count = 1;
        uint32 price = 0;  // copper for the whole deal; 0 = no price named
    };

    bool Parse(std::string const& text, Request& request);
}

// WTS/WTB market commands. `!wts <itemlink> [count] [price]` (the sender is
// selling; the bot may buy) and `!wtb <itemlink> [count] [price]` (the sender
// is buying; the bot may sell) evaluate the offer and, when a concrete sane
// price was named, commit a deal in TradeOfferMgr that TradeFulfillAction
// then completes through a real trade window. Without a price they answer
// with a quote. `!appraise` and `!sellables` are read-only queries;
// `!sellto`/`!buyfrom <player> <itemlink> [count] <price>` are the explicit
// commitment commands the LLM's commit_trade tool wraps.

class WtbAction : public InventoryAction
{
public:
    WtbAction(PlayerbotAI* botAI) : InventoryAction(botAI, "wtb") {}

    bool Execute(Event event) override;
};

class AppraiseAction : public InventoryAction
{
public:
    AppraiseAction(PlayerbotAI* botAI) : InventoryAction(botAI, "appraise") {}

    bool Execute(Event event) override;
};

class SellablesAction : public InventoryAction
{
public:
    SellablesAction(PlayerbotAI* botAI) : InventoryAction(botAI, "sellables") {}

    bool Execute(Event event) override;
};

class SellToAction : public InventoryAction
{
public:
    SellToAction(PlayerbotAI* botAI) : InventoryAction(botAI, "sellto") {}

    bool Execute(Event event) override;
};

class BuyFromAction : public InventoryAction
{
public:
    BuyFromAction(PlayerbotAI* botAI) : InventoryAction(botAI, "buyfrom") {}

    bool Execute(Event event) override;
};

#endif
