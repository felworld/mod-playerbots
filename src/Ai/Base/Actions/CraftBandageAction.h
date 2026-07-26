/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CRAFTBANDAGEACTION_H
#define PLAYERBOTS_CRAFTBANDAGEACTION_H

#include "Action.h"

class Player;
class PlayerbotAI;

// Stop crafting once the bot carries this many bandages.
constexpr uint32 CRAFT_BANDAGE_TARGET_COUNT = 20;

class CraftBandageAction : public Action
{
public:
    CraftBandageAction(PlayerbotAI* botAI) : Action(botAI, "craft bandage") {}

    bool Execute(Event event) override;
    bool isUseful() override;

    // Highest-level bandage spell the bot knows and has the cloth for; 0 if none.
    static uint32 FindBestBandageSpell(Player* bot);
    static uint32 BandageCount(Player* bot);
};

#endif
