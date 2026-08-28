/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CANCELIMMUNITYSTRATEGY_H
#define PLAYERBOTS_CANCELIMMUNITYSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

// Always on, in and out of combat: drops an immunity the bot is answerable for once it has outlived
// its use (see ImmunityTriggers.h) - one it is wearing, or the Banish it put on a mob.
// Out-of-combat coverage matters for Divine Intervention, which is meant to be cancelled after the
// wipe has settled (Felworld).
class CancelImmunityStrategy : public Strategy
{
public:
    CancelImmunityStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "cancel immunity"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
