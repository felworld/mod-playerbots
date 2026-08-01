/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRADEFULFILLACTION_H
#define PLAYERBOTS_TRADEFULFILLACTION_H

#include "MovementActions.h"

class PlayerbotAI;

// Completes a pending WTS/WTB deal queued in TradeOfferMgr: walk to the
// counterparty, open a trade window, put in the agreed goods (selling) or
// gold (buying), and accept - but only while the counterparty's side of the
// window actually matches the deal.
class TradeFulfillAction : public MovementAction
{
public:
    TradeFulfillAction(PlayerbotAI* botAI) : MovementAction(botAI, "fulfill trade deal") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
