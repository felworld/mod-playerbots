/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "BattleGroundTactics.h"
#include "Chat.h"
#include "ChatHelper.h"
#include "DBCStores.h"
#include "GuildTaskMgr.h"
#include "NewRpgWpvp.h"
#include "PerfMonitor.h"
#include "PlayerbotCommandScript.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "Random.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

// The enable/disable/status handlers are free functions declared in
// PlayerbotCommandScript.h so the unit tests can call them directly.

bool HandlePlayerbotsEnableCommand(ChatHandler* handler, char const* /*args*/)
{
    if (sPlayerbotAIConfig.enabled)
    {
        handler->PSendSysMessage("Playerbots are already enabled.");
        return true;
    }

    // Flip the master switch live. The random-bot tick
    // (RandomPlayerbotMgr::UpdateAIInternal) reads sPlayerbotAIConfig.enabled
    // every pass and resumes on its own, calling AddRandomBots() to refill
    // the population over the next few minutes. This is a runtime-only
    // override: a later config reload or restart re-reads AiPlayerbot.Enabled
    // from playerbots.conf.
    sPlayerbotAIConfig.enabled = true;
    handler->PSendSysMessage("Playerbots enabled. Random bots will repopulate over the next few minutes.");
    LOG_INFO("playerbots", "Playerbots enabled at runtime via .playerbots enable.");
    return true;
}

bool HandlePlayerbotsDisableCommand(ChatHandler* handler, char const* /*args*/)
{
    if (!sPlayerbotAIConfig.enabled)
    {
        handler->PSendSysMessage("Playerbots are already disabled.");
        return true;
    }

    sPlayerbotAIConfig.enabled = false;

    // Kick every random bot now. This reuses the same mass-logout the core
    // runs at shutdown and when no real players remain online. With
    // enabled = false the random-bot tick early-returns, so nothing
    // repopulates them until re-enabled. Player-summoned alt bots live in
    // separate per-player PlayerbotMgr holders and are not touched here;
    // the enabled gate simply stops new alt-bot logins.
    uint32 count = sRandomPlayerbotMgr.GetPlayerbotsCount();
    sRandomPlayerbotMgr.LogoutAllBots();

    handler->PSendSysMessage("Playerbots disabled. Logging out {} random bot(s).", count);
    LOG_INFO("playerbots", "Playerbots disabled at runtime via .playerbots disable ({} bots logged out).", count);
    return true;
}

bool HandlePlayerbotsStatusCommand(ChatHandler* handler, char const* /*args*/)
{
    handler->PSendSysMessage("Playerbots are currently {}. {} random bot(s) online.",
        sPlayerbotAIConfig.enabled ? "ENABLED" : "DISABLED",
        sRandomPlayerbotMgr.GetPlayerbotsCount());
    return true;
}

class playerbots_commandscript : public CommandScript
{
public:
    playerbots_commandscript() : CommandScript("playerbots_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable playerbotsDebugCommandTable = {
            {"bg", HandleDebugBGCommand, SEC_GAMEMASTER, Console::Yes},
        };

        static ChatCommandTable playerbotsAccountCommandTable = {
            {"setKey", HandleSetSecurityKeyCommand, SEC_PLAYER, Console::No},
            {"link", HandleLinkAccountCommand, SEC_PLAYER, Console::No},
            {"linkedAccounts", HandleViewLinkedAccountsCommand, SEC_PLAYER, Console::No},
            {"unlink", HandleUnlinkAccountCommand, SEC_PLAYER, Console::No},
        };

        static ChatCommandTable playerbotsWpvpCommandTable = {
            {"test", HandleWpvpTestCommand, SEC_GAMEMASTER, Console::No},
            {"on", HandleWpvpOnCommand, SEC_GAMEMASTER, Console::Yes},
            {"off", HandleWpvpOffCommand, SEC_GAMEMASTER, Console::Yes},
            {"status", HandleWpvpStatusCommand, SEC_GAMEMASTER, Console::Yes},
        };

        static ChatCommandTable playerbotsCommandTable = {
            {"bot", HandlePlayerbotCommand, SEC_PLAYER, Console::No},
            {"wpvp", playerbotsWpvpCommandTable},
            {"enable", HandlePlayerbotsEnableCommand, SEC_ADMINISTRATOR, Console::Yes},
            {"disable", HandlePlayerbotsDisableCommand, SEC_ADMINISTRATOR, Console::Yes},
            {"status", HandlePlayerbotsStatusCommand, SEC_ADMINISTRATOR, Console::Yes},
            {"gtask", HandleGuildTaskCommand, SEC_GAMEMASTER, Console::Yes},
            {"pmon", HandlePerfMonCommand, SEC_GAMEMASTER, Console::Yes},
            {"rndbot", HandleRandomPlayerbotCommand, SEC_GAMEMASTER, Console::Yes},
            {"debug", playerbotsDebugCommandTable},
            {"account", playerbotsAccountCommandTable},
        };

        static ChatCommandTable commandTable = {
            {"playerbots", playerbotsCommandTable},
        };

        return commandTable;
    }

    static bool HandlePlayerbotCommand(ChatHandler* handler, char const* args)
    {
        return PlayerbotMgr::HandlePlayerbotMgrCommand(handler, args);
    }

    static uint8 ParseClassName(std::string const& name)
    {
        static std::unordered_map<std::string, uint8> const classes = {
            {"warrior", CLASS_WARRIOR}, {"paladin", CLASS_PALADIN}, {"hunter", CLASS_HUNTER},
            {"rogue", CLASS_ROGUE},     {"priest", CLASS_PRIEST},   {"dk", CLASS_DEATH_KNIGHT},
            {"deathknight", CLASS_DEATH_KNIGHT},                    {"shaman", CLASS_SHAMAN},
            {"mage", CLASS_MAGE},       {"warlock", CLASS_WARLOCK}, {"druid", CLASS_DRUID},
        };
        auto it = classes.find(name);
        return it != classes.end() ? it->second : 0;
    }

    // Send a random opposing-faction bot on an excursion to the requester's
    // position. The handler only assigns the payload - the bot's own action
    // performs the guarded teleport and walk-in, so the real path gets
    // exercised end to end.
    static bool HandleWpvpTestCommand(ChatHandler* handler, char const* args)
    {
        Player* requester = handler->GetSession()->GetPlayer();
        if (!requester)
            return false;

        uint8 classFilter = 0;
        std::string arg = args ? args : "";
        if (!arg.empty())
        {
            classFilter = ParseClassName(arg);
            if (!classFilter)
            {
                handler->PSendSysMessage("Unknown class '{}'.", arg);
                return true;
            }
        }

        std::vector<Player*> candidates;
        std::vector<Player*> preferred;
        for (auto const& [guid, bot] : sRandomPlayerbotMgr.GetAllBots())
        {
            if (!bot || !bot->IsInWorld() || !bot->IsAlive())
                continue;

            if (!sRandomPlayerbotMgr.IsRandomBot(bot))
                continue;

            if (bot->GetTeamId() == requester->GetTeamId())
                continue;

            if (bot->InBattleground() || bot->InArena() || bot->GetGroup() || bot->IsInCombat())
                continue;

            if (classFilter && bot->getClass() != classFilter)
                continue;

            if (!GET_PLAYERBOT_AI(bot))
                continue;

            candidates.push_back(bot);
            if (std::abs(int32(bot->GetLevel()) - int32(requester->GetLevel())) <= 10)
                preferred.push_back(bot);
        }

        std::vector<Player*>& pool = preferred.empty() ? candidates : preferred;
        if (pool.empty())
        {
            handler->PSendSysMessage("No eligible opposing-faction bot found{}.",
                                     classFilter ? " for that class" : "");
            return true;
        }

        Player* bot = pool[urand(0, pool.size() - 1)];
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

        NewRpgInfo::GoWpvp payload;
        WorldLocation here(requester->GetMapId(), requester->GetPositionX(), requester->GetPositionY(),
                           requester->GetPositionZ(), requester->GetOrientation());
        if (!ComputeWpvpPositions(here, requester->GetZoneId(), payload))
        {
            handler->PSendSysMessage("Failed to compute excursion positions around you.");
            return true;
        }

        botAI->rpgInfo.ChangeToGoWpvp(std::move(payload));
        handler->PSendSysMessage("Sent {} (level {} {}) on a wpvp excursion to your position.", bot->GetName(),
                                 bot->GetLevel(), ChatHelper::FormatClass(bot->getClass()));
        handler->PSendSysMessage(
            "Note: stay more than 150yd from the arrival point or the nearby-player teleport guard will stall it.");
        return true;
    }

    static bool HandleWpvpOnCommand(ChatHandler* handler, char const* /*args*/)
    {
        sRandomPlayerbotMgr.SetWpvpDisabledUntil(0);
        handler->PSendSysMessage("World PvP excursions enabled.");
        return true;
    }

    static bool HandleWpvpOffCommand(ChatHandler* handler, char const* args)
    {
        uint32 minutes = sPlayerbotAIConfig.wpvpKillSwitchDefaultMinutes;
        std::string arg = args ? args : "";
        if (!arg.empty())
            minutes = atoi(arg.c_str());

        if (minutes == 0)
        {
            sRandomPlayerbotMgr.SetWpvpDisabledUntil(std::numeric_limits<time_t>::max());
            handler->PSendSysMessage("World PvP excursions disabled until server restart.");
        }
        else
        {
            sRandomPlayerbotMgr.SetWpvpDisabledUntil(time(nullptr) + time_t(minutes) * MINUTE);
            handler->PSendSysMessage("World PvP excursions disabled for {} minute(s).", minutes);
        }

        // Send everyone currently out on an excursion home.
        uint32 ended = 0;
        for (auto const& [guid, bot] : sRandomPlayerbotMgr.GetAllBots())
        {
            if (!bot)
                continue;

            PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
            if (!botAI || botAI->rpgInfo.GetStatus() != RPG_GO_WPVP)
                continue;

            EndWpvpExcursion(botAI);
            ++ended;
        }
        handler->PSendSysMessage("Ended {} active excursion(s).", ended);
        return true;
    }

    static bool HandleWpvpStatusCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (sRandomPlayerbotMgr.IsWpvpExcursionEnabled())
            handler->PSendSysMessage("World PvP excursions: ENABLED (weight {}).",
                                     sPlayerbotAIConfig.RpgStatusProbWeight[RPG_GO_WPVP]);
        else
        {
            time_t until = sRandomPlayerbotMgr.GetWpvpDisabledUntil();
            if (until == std::numeric_limits<time_t>::max())
                handler->PSendSysMessage("World PvP excursions: DISABLED until server restart.");
            else
                handler->PSendSysMessage("World PvP excursions: DISABLED for {} more minute(s).",
                                         (until - time(nullptr) + MINUTE - 1) / MINUTE);
        }

        uint32 count = 0;
        for (auto const& [guid, bot] : sRandomPlayerbotMgr.GetAllBots())
        {
            if (!bot)
                continue;

            PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
            if (!botAI)
                continue;

            auto* data = std::get_if<NewRpgInfo::GoWpvp>(&botAI->rpgInfo.data);
            if (!data)
                continue;

            ++count;
            AreaTableEntry const* zone = sAreaTableStore.LookupEntry(data->zoneId);
            std::string zoneName = PlayerbotAI::GetLocalizedAreaName(zone);
            handler->PSendSysMessage("  {} (level {} {}) -> {}: {}, {} death(s)", bot->GetName(), bot->GetLevel(),
                                     ChatHelper::FormatClass(bot->getClass()), zoneName,
                                     data->arrivedT ? "dwelling" : "travelling", data->deathCount);
        }
        handler->PSendSysMessage("{} bot(s) on world PvP excursions.", count);
        return true;
    }

    static bool HandleRandomPlayerbotCommand(ChatHandler* handler, char const* args)
    {
        return RandomPlayerbotMgr::HandlePlayerbotConsoleCommand(handler, args);
    }

    static bool HandleGuildTaskCommand(ChatHandler* handler, char const* args)
    {
        return GuildTaskMgr::HandleConsoleCommand(handler, args);
    }

    static bool HandlePerfMonCommand(ChatHandler* /*handler*/, char const* args)
    {
        if (!strcmp(args, "reset"))
        {
            sPerfMonitor.Reset();
            return true;
        }

        if (!strcmp(args, "tick"))
        {
            sPerfMonitor.PrintStats(true, false);
            return true;
        }

        if (!strcmp(args, "stack"))
        {
            sPerfMonitor.PrintStats(false, true);
            return true;
        }

        if (!strcmp(args, "toggle"))
        {
            sPlayerbotAIConfig.perfMonEnabled = !sPlayerbotAIConfig.perfMonEnabled;
            if (sPlayerbotAIConfig.perfMonEnabled)
                LOG_INFO("playerbots", "Performance monitor enabled");
            else
                LOG_INFO("playerbots", "Performance monitor disabled");
            return true;
        }

        sPerfMonitor.PrintStats();
        return true;
    }

    static bool HandleDebugBGCommand(ChatHandler* handler, char const* args)
    {
        return BGTactics::HandleConsoleCommand(handler, args);
    }

    static bool HandleSetSecurityKeyCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
        {
            handler->PSendSysMessage("Usage: .playerbots account setKey <securityKey>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        std::string key = args;

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleSetSecurityKeyCommand(player, key);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleLinkAccountCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
            return false;

        char* accountName = strtok((char*)args, " ");
        char* key = strtok(nullptr, " ");

        if (!accountName || !key)
        {
            handler->PSendSysMessage("Usage: .playerbots account link <accountName> <securityKey>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleLinkAccountCommand(player, accountName, key);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleViewLinkedAccountsCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleViewLinkedAccountsCommand(player);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleUnlinkAccountCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
            return false;

        char* accountName = strtok((char*)args, " ");
        if (!accountName)
        {
            handler->PSendSysMessage("Usage: .playerbots account unlink <accountName>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleUnlinkAccountCommand(player, accountName);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }
};

void AddPlayerbotsCommandscripts() { new playerbots_commandscript(); }
