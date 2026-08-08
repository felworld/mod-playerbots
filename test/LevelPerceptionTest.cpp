/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "LevelPerception.h"
#include "gtest/gtest.h"

// Level perception: a bot may only act on the level its owner could read off
// the target frame - an exact number for friendlies and for hostiles within
// the skull gap, and nothing but "at least this far above me" past it.

// cppcheck-suppress syntaxError
TEST(LevelPerceptionTest, FriendlyLevelsAreAlwaysReadable)
{
    EXPECT_TRUE(IsLevelKnown(10, 80, false));
    EXPECT_EQ(PerceivedLevel(10, 80, false), 80);
}

TEST(LevelPerceptionTest, HostileBelowTheGapIsReadable)
{
    EXPECT_TRUE(IsLevelKnown(20, 29, true));
    EXPECT_EQ(PerceivedLevel(20, 29, true), 29);
}

TEST(LevelPerceptionTest, HostileAtTheGapWearsASkull)
{
    EXPECT_FALSE(IsLevelKnown(20, 30, true));
    EXPECT_EQ(PerceivedLevel(20, 30, true), 30);
}

TEST(LevelPerceptionTest, SkullTellsOnlyTheFloor)
{
    // The classic case: a level 20 ganked by a level 80 knows "way above me",
    // not "80".
    EXPECT_FALSE(IsLevelKnown(20, 80, true));
    EXPECT_EQ(PerceivedLevel(20, 80, true), 30);
}

TEST(LevelPerceptionTest, LowerHostilesAreAlwaysReadable)
{
    EXPECT_TRUE(IsLevelKnown(80, 20, true));
    EXPECT_EQ(PerceivedLevel(80, 20, true), 20);
}

TEST(LevelPerceptionTest, SkullIsSpokenAsPlayersSpeakIt)
{
    EXPECT_EQ(LevelPhrase("28"), "level 28");
    EXPECT_EQ(LevelPhrase("??"), "??-level");
}

TEST(LevelPerceptionTest, FloorStillAnswersEveryGapTest)
{
    // Every level gate in the bots is smaller than the skull gap, so clamping
    // to the floor leaves their answers unchanged: a hidden level is still
    // unambiguously "more than the gank gap above me".
    uint8 const perceived = PerceivedLevel(20, 80, true);
    EXPECT_GT(int32(perceived) - 20, 5);
    EXPECT_GT(int32(perceived) - 20, 4);
    EXPECT_GT(int32(perceived) - 20, 3);
}
