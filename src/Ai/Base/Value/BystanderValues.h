/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_BYSTANDERVALUES_H
#define _PLAYERBOT_BYSTANDERVALUES_H

#include "ObjectGuid.h"
#include "Value.h"

#include <deque>
#include <unordered_map>
#include <vector>

class Player;
class PlayerbotAI;
class Unit;

// The bystander-assist rescue paths are gated by class, not spec: a feral
// druid can still drop form and throw a Healing Touch at a dying stranger.
bool IsBystanderHealerClass(Player* player);

// The nearby non-group player most in need of a rescue, or nullptr. Owns the
// per-player health-sample history that backs the rapid-loss distress
// criterion and the per-victim cooldown that stops assist ping-pong.
class BystanderToAssistValue : public UnitCalculatedValue
{
public:
    BystanderToAssistValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "bystander to assist") {}

    Unit* Calculate() override;

    void MarkAssisted(ObjectGuid victimGuid);

private:
    struct HealthSample
    {
        uint8 healthPct;
        uint32 timeMs;
    };

    void SampleHealth(Player* player, uint32 now);
    bool FindPreviousSample(ObjectGuid guid, uint32 now, uint8& prevHealthPct, uint32& prevAgeMs) const;
    void Prune(uint32 now);
    bool IsWinnable(Player* victim, std::vector<Unit*> const& attackers) const;

    std::unordered_map<ObjectGuid, std::deque<HealthSample>> _samples;
    std::unordered_map<ObjectGuid, uint32> _cooldownEndMs;
    uint32 _lastPruneMs = 0;
};

// The creature attacking the bystander that the bot should engage, or
// nullptr. Only ever returns creatures: rescuing someone from another player
// is out of scope, and that rule is structural here.
class BystanderAttackerValue : public UnitCalculatedValue
{
public:
    BystanderAttackerValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "bystander attacker") {}

    Unit* Calculate() override;
};

#endif
