/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "BotCommandPrefix.h"
#include "gtest/gtest.h"

#include <string>

// AiPlayerbot.CommandPrefix semantics: with a prefix configured, only messages
// starting with it are bot commands; everything else stays ordinary chat.

// cppcheck-suppress syntaxError
TEST(BotCommandPrefixTest, EmptyPrefixTreatsEveryMessageAsACommand)
{
    std::string text = "follow";
    EXPECT_TRUE(StripBotCommandPrefix(text, ""));
    EXPECT_EQ(text, "follow");
}

TEST(BotCommandPrefixTest, PrefixedMessageIsACommandWithThePrefixStripped)
{
    std::string text = "!follow";
    EXPECT_TRUE(StripBotCommandPrefix(text, "!"));
    EXPECT_EQ(text, "follow");
}

TEST(BotCommandPrefixTest, UnprefixedMessageIsOrdinaryChat)
{
    std::string text = "follow";
    EXPECT_FALSE(StripBotCommandPrefix(text, "!"));
    EXPECT_EQ(text, "follow");
}

TEST(BotCommandPrefixTest, SentenceStartingWithACommandWordIsOrdinaryChat)
{
    // The motivating case: without the prefix requirement, "who ..." would be
    // swallowed by the `who` command instead of getting a chat reply.
    std::string text = "who said that?";
    EXPECT_FALSE(StripBotCommandPrefix(text, "!"));
    EXPECT_EQ(text, "who said that?");
}

TEST(BotCommandPrefixTest, PrefixLaterInTheMessageDoesNotCount)
{
    std::string text = "hey !follow";
    EXPECT_FALSE(StripBotCommandPrefix(text, "!"));
    EXPECT_EQ(text, "hey !follow");
}

TEST(BotCommandPrefixTest, MultiCharacterPrefixIsStrippedWhole)
{
    std::string text = "bot:attack";
    EXPECT_TRUE(StripBotCommandPrefix(text, "bot:"));
    EXPECT_EQ(text, "attack");
}
