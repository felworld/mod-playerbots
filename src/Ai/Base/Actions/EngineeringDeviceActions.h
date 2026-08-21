/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ENGINEERINGDEVICEACTIONS_H
#define PLAYERBOTS_ENGINEERINGDEVICEACTIONS_H

#include "Action.h"

#include <vector>

class Item;
class Player;
class PlayerbotAI;

// The engineer trinket-box devices bots know how to use, shared with PlayerbotFactory stocking.
namespace EngineeringDevices
{
    struct Tier
    {
        uint32 itemId;
        uint32 rank;  // required Engineering skill
    };

    std::vector<Tier> const& TargetDummies();
    std::vector<Tier> const& JumperCables();
    std::vector<Tier> const& ExplosiveSheep();

    // Highest tier the skill allows; 0 if none.
    uint32 BestForSkill(std::vector<Tier> const& ladder, uint32 skill);

    // Best carried, usable, off-cooldown device from the ladder; nullptr if none.
    Item* FindBestCarried(Player* bot, std::vector<Tier> const& ladder);

    // Whether dropping a target dummy could actually peel something off the bot: it works by
    // taunting, which only units with a threat list obey, and it is barred inside instances.
    bool TargetDummyWouldHelp(Player* bot);
}

// Drop a target dummy to shed aggro when the bot is getting overwhelmed.
class UseTargetDummyAction : public Action
{
public:
    UseTargetDummyAction(PlayerbotAI* botAI) : Action(botAI, "target dummy") {}

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;
};

// Release an explosive sheep at the enemy, because engineering.
class UseExplosiveSheepAction : public Action
{
public:
    UseExplosiveSheepAction(PlayerbotAI* botAI) : Action(botAI, "explosive sheep") {}

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;
};

// Try to jump-start a dead group member when the bot has no real resurrection spell.
class UseJumperCablesAction : public Action
{
public:
    UseJumperCablesAction(PlayerbotAI* botAI) : Action(botAI, "jumper cables") {}

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;
};

#endif
