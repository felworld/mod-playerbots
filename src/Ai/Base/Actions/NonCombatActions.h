/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef PLAYERBOTS_NONCOMBATACTIONS_H
#define PLAYERBOTS_NONCOMBATACTIONS_H

#include "UseItemAction.h"

class Player;
class PlayerbotAI;

namespace BotConsumables
{
bool IsEatingFood(Player* bot);
bool IsDrinking(Player* bot);
bool IsSafeToConsumeInBattleground(PlayerbotAI* botAI, Player* bot);
}

class DrinkAction : public UseItemAction
{
public:
    DrinkAction(PlayerbotAI* botAI) : UseItemAction(botAI, "drink") {}

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;
};

class EatAction : public UseItemAction
{
public:
    EatAction(PlayerbotAI* botAI) : UseItemAction(botAI, "food") {}

    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;
};

class ContinueEatingAction : public Action
{
public:
    ContinueEatingAction(PlayerbotAI* botAI) : Action(botAI, "continue eating") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
