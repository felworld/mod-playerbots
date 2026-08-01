/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRADEFULFILLACTION_H
#define PLAYERBOTS_TRADEFULFILLACTION_H

#include "NewRpgBaseAction.h"

class PlayerbotAI;
class Player;
struct PendingTradeDeal;

// Completes a pending WTS/WTB deal queued in TradeOfferMgr: walk to the
// counterparty, open a trade window, put in the agreed goods (selling) or
// gold (buying), and accept - but only while the counterparty's side of the
// window actually matches the deal. A cross-city deal first travels: wait
// out the simulated ride, teleport near the counterparty while unobserved
// at both ends (the WPvP-excursion trick), then walk the last stretch in.
class TradeFulfillAction : public NewRpgBaseAction
{
public:
    TradeFulfillAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "fulfill trade deal") {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    bool TravelTo(Player* counterparty, PendingTradeDeal const& deal);
};

#endif
