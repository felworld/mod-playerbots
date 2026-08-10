/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "BystanderDistress.h"
#include "gtest/gtest.h"

// Bystander-assist distress predicate: a solo bot rescues a nearby stranger
// only when they look like they're about to die, not on every scratched mob
// fight.

namespace
{
    BystanderDistressConfig DefaultConfig()
    {
        BystanderDistressConfig config;
        config.distressHealth = 40;
        config.distressHealerHealth = 25;
        config.rateHealthLoss = 20;
        config.rateWindowMs = 4000;
        config.distressMobCount = 3;
        config.swarmHealthCeiling = 85;
        config.lowMana = 15;
        config.lowManaHealth = 60;
        return config;
    }

    BystanderSnapshot Fighting(uint8_t healthPct, uint8_t attackers = 1)
    {
        BystanderSnapshot s = {};
        s.healthPct = healthPct;
        s.inCombat = true;
        s.creatureAttackerCount = attackers;
        return s;
    }
}

// cppcheck-suppress syntaxError
TEST(BystanderDistressTest, ComfortableFightIsNotDistress)
{
    EXPECT_FALSE(IsBystanderInDistress(Fighting(65), DefaultConfig()));
}

TEST(BystanderDistressTest, OutOfCombatIsNeverDistress)
{
    BystanderSnapshot s = Fighting(10);
    s.inCombat = false;
    EXPECT_FALSE(IsBystanderInDistress(s, DefaultConfig()));
}

TEST(BystanderDistressTest, NoAttackersIsNeverDistress)
{
    // In combat with nobody actually on them (attackers all dead or gone).
    BystanderSnapshot s = Fighting(10, 0);
    EXPECT_FALSE(IsBystanderInDistress(s, DefaultConfig()));
}

TEST(BystanderDistressTest, PlayerAttackersQualifyForDistress)
{
    // Losing a PvP fight counts (issue #53): the PvP-support path heals
    // strangers under player attack by the same distress criteria.
    BystanderSnapshot s = Fighting(10, 0);
    s.playerAttackerCount = 1;
    EXPECT_TRUE(IsBystanderInDistress(s, DefaultConfig()));

    s.healthPct = 65;
    EXPECT_FALSE(IsBystanderInDistress(s, DefaultConfig()));
}

TEST(BystanderDistressTest, MixedSwarmCountsBothKinds)
{
    // Two mobs plus a player dogpiling counts as a swarm of three.
    BystanderSnapshot s = Fighting(80, 2);
    s.playerAttackerCount = 1;
    EXPECT_TRUE(IsBystanderInDistress(s, DefaultConfig()));
}

TEST(BystanderDistressTest, DrainedManaUserIsDistressAtModerateHealth)
{
    // The motivating incident: a caster out of mana at half health is doomed
    // long before a raw HP threshold would fire.
    BystanderSnapshot s = Fighting(55);
    s.hasMana = true;
    s.manaPct = 5;
    EXPECT_TRUE(IsBystanderInDistress(s, DefaultConfig()));
}

TEST(BystanderDistressTest, DrainedManaUserAtHighHealthIsNotDistress)
{
    BystanderSnapshot s = Fighting(90);
    s.hasMana = true;
    s.manaPct = 5;
    EXPECT_FALSE(IsBystanderInDistress(s, DefaultConfig()));
}

TEST(BystanderDistressTest, LowHealthIsDistress)
{
    EXPECT_TRUE(IsBystanderInDistress(Fighting(35), DefaultConfig()));
}

TEST(BystanderDistressTest, HealerWithManaNeedsAStrongerSignal)
{
    BystanderSnapshot s = Fighting(35);
    s.isHealerCapableClass = true;
    s.hasMana = true;
    s.manaPct = 80;
    EXPECT_FALSE(IsBystanderInDistress(s, DefaultConfig()));

    s.healthPct = 20;
    EXPECT_TRUE(IsBystanderInDistress(s, DefaultConfig()));
}

TEST(BystanderDistressTest, DrainedHealerIsTreatedLikeAnyoneElse)
{
    BystanderSnapshot s = Fighting(35);
    s.isHealerCapableClass = true;
    s.hasMana = true;
    s.manaPct = 5;
    EXPECT_TRUE(IsBystanderInDistress(s, DefaultConfig()));
}

TEST(BystanderDistressTest, RapidHealthLossIsDistress)
{
    BystanderSnapshot s = Fighting(70);
    s.hasPrevSample = true;
    s.prevHealthPct = 95;
    s.prevSampleAgeMs = 3000;
    EXPECT_TRUE(IsBystanderInDistress(s, DefaultConfig()));
}

TEST(BystanderDistressTest, StaleSampleDoesNotCountAsRapidLoss)
{
    BystanderSnapshot s = Fighting(70);
    s.hasPrevSample = true;
    s.prevHealthPct = 95;
    s.prevSampleAgeMs = 6000;
    EXPECT_FALSE(IsBystanderInDistress(s, DefaultConfig()));
}

TEST(BystanderDistressTest, RecoveringHealthIsNotRapidLoss)
{
    BystanderSnapshot s = Fighting(70);
    s.hasPrevSample = true;
    s.prevHealthPct = 50;
    s.prevSampleAgeMs = 3000;
    EXPECT_FALSE(IsBystanderInDistress(s, DefaultConfig()));
}

TEST(BystanderDistressTest, SwarmedAndDamagedIsDistress)
{
    EXPECT_TRUE(IsBystanderInDistress(Fighting(80, 3), DefaultConfig()));
}

TEST(BystanderDistressTest, SwarmedAtFullHealthIsNotDistress)
{
    // An AoE grinder pulling a pack on purpose hasn't taken damage yet.
    EXPECT_FALSE(IsBystanderInDistress(Fighting(100, 3), DefaultConfig()));
}

TEST(BystanderDistressTest, TwoAttackersAreNotASwarm)
{
    EXPECT_FALSE(IsBystanderInDistress(Fighting(80, 2), DefaultConfig()));
}

TEST(BystanderDistressTest, FoePowerScalesWithLevelDelta)
{
    EXPECT_EQ(BystanderFoePower(-10), 0u);
    EXPECT_EQ(BystanderFoePower(-9), 10u);
    EXPECT_EQ(BystanderFoePower(-3), 70u);
    EXPECT_EQ(BystanderFoePower(0), 100u);
    EXPECT_EQ(BystanderFoePower(3), 600u);
}

TEST(BystanderDistressTest, FoePowerScalesWithRankFactor)
{
    EXPECT_EQ(BystanderFoePower(0, 300), 300u);  // same-level elite
    EXPECT_EQ(BystanderFoePower(0, 200), 200u);  // same-level rare
    EXPECT_EQ(BystanderFoePower(-10, 300), 0u);  // gray elites still don't count
}

TEST(BystanderDistressTest, EliteCompositionMath)
{
    // Two same-level players muster 400: enough for one elite (300) or an
    // elite plus one add (400), but not two elites (600) or elite + two
    // adds (500).
    uint32_t twoPlayers = BYSTANDER_SELF_BASE_POWER + BystanderFriendPower(0);
    uint32_t elite = BystanderFoePower(0, 300);
    uint32_t add = BystanderFoePower(0);

    EXPECT_LE(elite, twoPlayers);
    EXPECT_LE(elite + add, twoPlayers);
    EXPECT_GT(elite + elite, twoPlayers);
    EXPECT_GT(elite + add + add, twoPlayers);
}

TEST(BystanderDistressTest, FriendPowerScalesWithLevelDelta)
{
    EXPECT_EQ(BystanderFriendPower(-10), 0u);
    EXPECT_EQ(BystanderFriendPower(-9), 20u);
    EXPECT_EQ(BystanderFriendPower(-3), 140u);
    EXPECT_EQ(BystanderFriendPower(0), 200u);
    EXPECT_EQ(BystanderFriendPower(3), 600u);
}
