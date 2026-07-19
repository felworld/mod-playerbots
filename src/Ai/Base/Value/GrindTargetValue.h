/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef PLAYERBOTS_GRINDTARGETVALUE_H
#define PLAYERBOTS_GRINDTARGETVALUE_H

#include "TargetValue.h"

class PlayerbotAI;
class Unit;

class Player;

class GrindTargetValue : public TargetValue
{
public:
    GrindTargetValue(PlayerbotAI* botAI, std::string const name = "grind target") : TargetValue(botAI, name) {}

    Unit* Calculate() override;

    // Does <player> still need creatures of this entry for an in-progress
    // quest, either as an unfinished kill objective or as a quest-item drop?
    static bool PlayerNeedsCreatureForQuest(Player* player, uint32 creatureEntry);

protected:
    virtual bool QuestTargetsOnly() const { return false; }

private:
    uint32 GetTargetingPlayerCount(Unit* unit);
    Unit* FindTargetForGrinding(uint32 assistCount);
    bool needForQuest(Unit* target);
    bool needForQuest(Player* player, Unit* target);
    bool groupNeedForQuest(Unit* target);
};

class QuestGrindTargetValue : public GrindTargetValue
{
public:
    QuestGrindTargetValue(PlayerbotAI* botAI) : GrindTargetValue(botAI, "quest grind target") {}

protected:
    bool QuestTargetsOnly() const override { return true; }
};

#endif
