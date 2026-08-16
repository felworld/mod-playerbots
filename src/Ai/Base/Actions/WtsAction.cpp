/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "WtsAction.h"
#include "AiFactory.h"
#include "ChatHelper.h"
#include "Event.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "TradeDealActions.h"
#include "TradeOfferMgr.h"

#include <algorithm>

// `!wts <itemlink> [count] [price]`: the sender is selling; the bot answers
// as a buyer. With a sane concrete price the deal commits and the bot walks
// over to complete it through a real trade window; without one it quotes
// what it would pay. (Upstream this action only whispered a vendor-price
// offer it never honored.)
bool WtsAction::Execute(Event event)
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
    if (!appraisal.wants || !appraisal.bidEach)
    {
        bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                         "trade_wts_no_thanks", "Thanks, but %item is %reason.",
                         {{"%item", item}, {"%reason", appraisal.reason}}),
                     LANG_UNIVERSAL, owner);
        return true;
    }

    uint32 budget = MarketQuote::SpendableMoney(botAI);
    uint32 offer = uint32(std::min<uint64>(uint64(appraisal.bidEach) * request.count, budget));
    if (!offer)
    {
        bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                         "trade_wts_broke", "I could use %item, but I'm flat broke right now.",
                         {{"%item", item}}),
                     LANG_UNIVERSAL, owner);
        return true;
    }

    if (request.price)
    {
        std::string error;
        if (MarketQuote::Commit(botAI, owner, request.proto, request.count, request.price, false, error))
            return true;

        bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                         "trade_wts_counter", "Can't do that price for %item - best I can offer is %money.",
                         {{"%item", ChatHelper::FormatItem(request.proto, request.count > 1 ? request.count : 0)},
                          {"%money", ChatHelper::formatMoney(offer)}}),
                     LANG_UNIVERSAL, owner);
        return true;
    }

    bot->Whisper(PlayerbotTextMgr::instance().GetBotTextOrDefault(
                     "trade_wts_offer", "I'll buy %item for %money. Deal?",
                     {{"%item", ChatHelper::FormatItem(request.proto, request.count > 1 ? request.count : 0)},
                      {"%money", ChatHelper::formatMoney(offer)}}),
                 LANG_UNIVERSAL, owner);
    return true;
}
