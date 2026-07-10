/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "Chat.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotCommandScript.h"
#include "RandomPlayerbotMgr.h"
#include "gtest/gtest.h"

#include <string>
#include <vector>

namespace
{
void CollectPrintedLine(void* arg, std::string_view text)
{
    static_cast<std::vector<std::string>*>(arg)->emplace_back(text);
}

// .playerbots enable|disable|status: flips the module master switch live and
// reports the current state, without a restart.
class PlayerbotsToggleCommandTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        _originalEnabled = sPlayerbotAIConfig.enabled;
        _originalAutologin = sPlayerbotAIConfig.randomBotAutologin;

        // Both gates must be off before sRandomPlayerbotMgr is first touched,
        // so constructing the singleton cannot start the bot command server.
        sPlayerbotAIConfig.enabled = false;
        sPlayerbotAIConfig.randomBotAutologin = false;
        sRandomPlayerbotMgr.GetPlayerbotsCount();
    }

    void TearDown() override
    {
        sPlayerbotAIConfig.enabled = _originalEnabled;
        sPlayerbotAIConfig.randomBotAutologin = _originalAutologin;
    }

    bool Invoke(bool (*handlerFn)(ChatHandler*, char const*))
    {
        _output.clear();
        CliHandler handler(&_output, &CollectPrintedLine);
        return handlerFn(&handler, "");
    }

    std::string Output() const
    {
        std::string joined;
        for (std::string const& line : _output)
            joined += line;
        return joined;
    }

    std::vector<std::string> _output;
    bool _originalEnabled = false;
    bool _originalAutologin = false;
};

// cppcheck-suppress syntaxError
TEST_F(PlayerbotsToggleCommandTest, EnableTurnsPlayerbotsOn)
{
    sPlayerbotAIConfig.enabled = false;

    EXPECT_TRUE(Invoke(HandlePlayerbotsEnableCommand));
    EXPECT_TRUE(sPlayerbotAIConfig.enabled);
    EXPECT_NE(Output().find("Playerbots enabled"), std::string::npos);
}

TEST_F(PlayerbotsToggleCommandTest, EnableIsANoOpWhenAlreadyEnabled)
{
    sPlayerbotAIConfig.enabled = true;

    EXPECT_TRUE(Invoke(HandlePlayerbotsEnableCommand));
    EXPECT_TRUE(sPlayerbotAIConfig.enabled);
    EXPECT_NE(Output().find("already enabled"), std::string::npos);
}

TEST_F(PlayerbotsToggleCommandTest, DisableTurnsPlayerbotsOff)
{
    sPlayerbotAIConfig.enabled = true;

    EXPECT_TRUE(Invoke(HandlePlayerbotsDisableCommand));
    EXPECT_FALSE(sPlayerbotAIConfig.enabled);
    // No random bots are online in the test binary.
    EXPECT_NE(Output().find("Logging out 0 random bot(s)"), std::string::npos);
}

TEST_F(PlayerbotsToggleCommandTest, DisableIsANoOpWhenAlreadyDisabled)
{
    sPlayerbotAIConfig.enabled = false;

    EXPECT_TRUE(Invoke(HandlePlayerbotsDisableCommand));
    EXPECT_FALSE(sPlayerbotAIConfig.enabled);
    EXPECT_NE(Output().find("already disabled"), std::string::npos);
}

TEST_F(PlayerbotsToggleCommandTest, StatusReportsTheCurrentState)
{
    sPlayerbotAIConfig.enabled = false;
    EXPECT_TRUE(Invoke(HandlePlayerbotsStatusCommand));
    EXPECT_NE(Output().find("DISABLED"), std::string::npos);

    sPlayerbotAIConfig.enabled = true;
    EXPECT_TRUE(Invoke(HandlePlayerbotsStatusCommand));
    EXPECT_NE(Output().find("ENABLED"), std::string::npos);
}
}
