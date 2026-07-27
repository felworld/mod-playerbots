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
    // Gray recipes are skipped unless they are the best bandage recipe the bot knows.
    static uint32 FindBestBandageSpell(Player* bot);
    // Item level of the bandage a create-item spell produces; 0 if it is not a bandage spell.
    static uint32 BandageItemLevel(uint32 spellId);
    static uint32 BandageCount(Player* bot);
    static uint32 LowerTierBandageCount(Player* bot, uint32 itemLevel);
};

#endif
