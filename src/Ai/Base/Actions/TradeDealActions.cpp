/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TradeDealActions.h"

#include "ChatHelper.h"
#include "Event.h"
#include "ItemUsageValue.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "StringFormat.h"
#include "TradeOfferMgr.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace TradeDealParse
{
bool Parse(std::string const& text, Request& request)
{
    ItemIds ids = ChatHelper::parseItems(text);
    if (ids.empty())
        return false;

    request.proto = sObjectMgr->GetItemTemplate(*ids.begin());
    if (!request.proto)
        return false;

    std::istringstream stream(text);
    std::string token;
    while (stream >> token)
    {
        if (std::all_of(token.begin(), token.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
        {
            uint32 count = atoi(token.c_str());
            if (count)
                request.count = std::min<uint32>(count, 1000);
        }
        else if (uint32 money = ChatHelper::parseMoney(token))
            request.price = money;
    }

    return true;
}
}

bool WtbAction::Execute(Event event)
{
    Player* owner = event.getOwner();
    if (!owner)
        return false;

    if (!sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    TradeDealParse::Request request;
    if (!TradeDealParse::Parse(event.getParam(), request))
        return false;

    sTradeOfferMgr->RenewAdAnchor(bot->GetGUID());

    std::string item = ChatHelper::FormatItem(request.proto);
    MarketQuote::Appraisal appraisal = MarketQuote::Appraise(botAI, request.proto);
    if (appraisal.wants)
    {
        bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                         "trade_wtb_need_it", "Sorry, I need my %item myself.", {{"%item", item}}),
                     LANG_UNIVERSAL, owner);
        return true;
    }

    if (!appraisal.stock || !appraisal.askEach)
    {
        bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                         "trade_wtb_none_left", "I don't have %item to sell.", {{"%item", item}}),
                     LANG_UNIVERSAL, owner);
        return true;
    }

    uint32 count = std::min(request.count, appraisal.stock);

    if (request.price && request.count <= appraisal.stock)
    {
        std::string error;
        if (MarketQuote::Commit(botAI, owner, request.proto, count, request.price, true, error))
            return true;
    }

    bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                     "trade_wtb_quote", "I can sell you %item - %money each. Name the deal and I'll bring it.",
                     {{"%item", ChatHelper::FormatItem(request.proto, count > 1 ? count : 0)},
                      {"%money", ChatHelper::formatMoney(appraisal.askEach)}}),
                 LANG_UNIVERSAL, owner);
    return true;
}

bool AppraiseAction::Execute(Event event)
{
    Player* owner = event.getOwner();
    if (!owner)
        return false;

    TradeDealParse::Request request;
    if (!TradeDealParse::Parse(event.getParam(), request))
        return false;

    sTradeOfferMgr->RenewAdAnchor(bot->GetGUID());

    std::string item = ChatHelper::FormatItem(request.proto);
    MarketQuote::Appraisal appraisal = MarketQuote::Appraise(botAI, request.proto);

    std::string reply;
    if (appraisal.wants && appraisal.bidEach)
    {
        uint32 budget = MarketQuote::SpendableMoney(botAI);
        reply = PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "trade_appraise_wants", "%item? That's %reason - I'd pay around %money each.",
            {{"%item", item}, {"%reason", appraisal.reason},
             {"%money", ChatHelper::formatMoney(std::min(appraisal.bidEach, std::max<uint32>(budget, 1)))}});
    }
    else if (appraisal.stock && appraisal.askEach)
        reply = PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "trade_appraise_stock", "%item is %reason - I've got %count to spare at %money each.",
            {{"%item", item}, {"%reason", appraisal.reason},
             {"%count", std::to_string(appraisal.stock)},
             {"%money", ChatHelper::formatMoney(appraisal.askEach)}});
    else
        reply = PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "trade_appraise_none", "%item is %reason.", {{"%item", item}, {"%reason", appraisal.reason}});

    bot->Whisper(reply, LANG_UNIVERSAL, owner);
    return true;
}

bool SellablesAction::Execute(Event event)
{
    Player* owner = event.getOwner();
    if (!owner)
        return false;

    std::vector<MarketQuote::Sellable> sellables = MarketQuote::CollectSellables(botAI);
    std::vector<MarketQuote::Want> wants = MarketQuote::CollectWants(botAI);

    auto whisperChunks = [this, owner](std::string const& prefix, std::vector<std::string> const& entries)
    {
        std::string line;
        uint32 inLine = 0;
        for (std::string const& entry : entries)
        {
            line += line.empty() ? prefix + entry : ", " + entry;
            if (++inLine == 3)
            {
                bot->Whisper(line, LANG_UNIVERSAL, owner);
                line.clear();
                inLine = 0;
            }
        }
        if (!line.empty())
            bot->Whisper(line, LANG_UNIVERSAL, owner);
    };

    std::vector<std::string> selling;
    for (MarketQuote::Sellable const& sellable : sellables)
    {
        if (selling.size() >= 9)
            break;
        selling.push_back(Acore::StringFormat("{} ({} each)",
            ChatHelper::FormatItem(sellable.proto, sellable.count > 1 ? sellable.count : 0),
            ChatHelper::formatMoney(sellable.askEach)));
    }

    std::vector<std::string> buying;
    for (MarketQuote::Want const& want : wants)
    {
        if (buying.size() >= 6)
            break;
        buying.push_back(Acore::StringFormat("{} ({} each)",
            ChatHelper::FormatItem(want.proto), ChatHelper::formatMoney(want.bidEach)));
    }

    if (selling.empty() && buying.empty())
    {
        bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                         "trade_sellables_none", "Nothing to buy or sell right now.", {}),
                     LANG_UNIVERSAL, owner);
        return true;
    }

    if (!selling.empty())
        whisperChunks("Selling: ", selling);
    if (!buying.empty())
        whisperChunks("Buying: ", buying);
    return true;
}

bool SellToAction::Execute(Event event)
{
    std::string const text = event.getParam();
    size_t space = text.find(' ');
    if (space == std::string::npos)
        return false;

    Player* counterparty = ObjectAccessor::FindPlayerByName(text.substr(0, space));

    TradeDealParse::Request request;
    if (!TradeDealParse::Parse(text.substr(space + 1), request))
        return false;

    std::string error;
    if (MarketQuote::Commit(botAI, counterparty, request.proto, request.count, request.price, true, error))
        return true;

    if (Player* owner = event.getOwner())
        bot->Whisper("No deal - " + error + ".", LANG_UNIVERSAL, owner);
    return false;
}

bool BuyFromAction::Execute(Event event)
{
    std::string const text = event.getParam();
    size_t space = text.find(' ');
    if (space == std::string::npos)
        return false;

    Player* counterparty = ObjectAccessor::FindPlayerByName(text.substr(0, space));

    TradeDealParse::Request request;
    if (!TradeDealParse::Parse(text.substr(space + 1), request))
        return false;

    std::string error;
    if (MarketQuote::Commit(botAI, counterparty, request.proto, request.count, request.price, false, error))
        return true;

    if (Player* owner = event.getOwner())
        bot->Whisper("No deal - " + error + ".", LANG_UNIVERSAL, owner);
    return false;
}
