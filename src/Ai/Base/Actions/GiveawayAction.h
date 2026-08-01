/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_GIVEAWAYACTION_H
#define PLAYERBOTS_GIVEAWAYACTION_H

#include "MovementActions.h"

class PlayerbotAI;

// Completes a pending roll-win giveaway queued by GiveawayMgr: walk to the
// recipient, open a trade, put the won item in, announce it, and accept.
class GiveawayAction : public MovementAction
{
public:
    GiveawayAction(PlayerbotAI* botAI) : MovementAction(botAI, "give away roll win") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
