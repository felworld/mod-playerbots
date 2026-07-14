/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_BYSTANDERDISTRESS_H
#define _PLAYERBOT_BYSTANDERDISTRESS_H

#include <algorithm>
#include <cstdint>

// Pure decision logic for the "bystander assist" feature: solo bots rescuing
// nearby non-group players who look like they're about to die. Kept free of
// core includes so the predicate is unit-testable without a live world
// (see test/BystanderDistressTest.cpp).

struct BystanderDistressConfig
{
    uint8_t distressHealth;       // static HP% threshold
    uint8_t distressHealerHealth; // stronger threshold for healer classes with mana left
    uint8_t rateHealthLoss;       // % of max HP lost within rateWindowMs -> distress
    uint32_t rateWindowMs;
    uint8_t distressMobCount;     // attacker count for the swarm criterion
    uint8_t swarmHealthCeiling;   // swarm only counts once they've actually taken damage
    uint8_t lowMana;              // victim mana% considered "drained"
    uint8_t lowManaHealth;        // HP ceiling for the drained-caster rule
};

struct BystanderSnapshot
{
    uint8_t healthPct;
    bool hasMana;                 // victim's power type is mana
    uint8_t manaPct;              // meaningful only when hasMana
    bool isHealerCapableClass;    // priest / paladin / druid / shaman
    bool inCombat;
    uint8_t creatureAttackerCount;
    bool hasPrevSample;
    uint8_t prevHealthPct;        // meaningful only when hasPrevSample
    uint32_t prevSampleAgeMs;     // meaningful only when hasPrevSample
};

// "Seems like they're going to die": under creature attack AND any of a
// drained mana pool, low health, rapid health loss, or a mob swarm. Healer
// classes with mana remaining can usually save themselves, so they need a
// stronger health signal.
inline bool IsBystanderInDistress(BystanderSnapshot const& s, BystanderDistressConfig const& c)
{
    if (!s.inCombat || !s.creatureAttackerCount)
        return false;

    if (s.hasMana && s.manaPct < c.lowMana && s.healthPct < c.lowManaHealth)
        return true;

    bool healerWithMana = s.isHealerCapableClass && s.hasMana && s.manaPct >= c.lowMana;
    if (s.healthPct < (healerWithMana ? c.distressHealerHealth : c.distressHealth))
        return true;

    if (s.hasPrevSample && s.prevSampleAgeMs <= c.rateWindowMs && s.prevHealthPct > s.healthPct &&
        uint8_t(s.prevHealthPct - s.healthPct) >= c.rateHealthLoss)
        return true;

    if (s.creatureAttackerCount >= c.distressMobCount && s.healthPct < c.swarmHealthCeiling)
        return true;

    return false;
}

// Level-scaled power estimates, same arithmetic as OutNumberedTrigger
// (GenericTriggers.cpp): units more than 9 levels below contribute nothing.

inline uint32_t BystanderFoePower(int32_t levelDelta) // attacker level - bot level
{
    return levelDelta > -10 ? uint32_t(std::max(100 + 10 * levelDelta, levelDelta * 200)) : 0;
}

inline uint32_t BystanderFriendPower(int32_t levelDelta) // helper level - bot level
{
    return levelDelta > -10 ? uint32_t(std::max(200 + 20 * levelDelta, levelDelta * 200)) : 0;
}

constexpr uint32_t BYSTANDER_SELF_BASE_POWER = 200;

#endif
