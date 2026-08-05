/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BattleGroundTactics.h"
#include "Chat.h"
#include "ChatHelper.h"
#include "DBCStores.h"
#include "GuildTaskMgr.h"
#include "NewRpgWpvp.h"
#include "PerfMonitor.h"
#include "PlayerbotCommandScript.h"
#include "PlayerbotFactory.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "Random.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptMgr.h"

#include <cctype>

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
            {"gear", HandlePlayerbotsGearCommand, SEC_GAMEMASTER, Console::Yes},
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

    // Non-destructively re-gear an online character (human or bot) with
    // factory-picked items of the requested quality, using the same
    // spec-aware selection the bot "autogear" chat command uses. Replaced
    // items are moved into the character's bags by InitEquipment; a slot
    // whose old item does not fit in the bags is left unchanged.
    static bool HandlePlayerbotsGearCommand(ChatHandler* handler, Optional<PlayerIdentifier> target,
                                            std::string qualityName, Optional<uint32> maxItemLevel)
    {
        static std::unordered_map<std::string, uint32> const qualities = {
            {"white", ITEM_QUALITY_NORMAL},        {"common", ITEM_QUALITY_NORMAL},
            {"green", ITEM_QUALITY_UNCOMMON},      {"uncommon", ITEM_QUALITY_UNCOMMON},
            {"blue", ITEM_QUALITY_RARE},           {"rare", ITEM_QUALITY_RARE},
            {"epic", ITEM_QUALITY_EPIC},           {"purple", ITEM_QUALITY_EPIC},
            {"legendary", ITEM_QUALITY_LEGENDARY}, {"yellow", ITEM_QUALITY_LEGENDARY},
        };

        for (char& c : qualityName)
            c = std::tolower(c);

        auto it = qualities.find(qualityName);
        if (it == qualities.end())
        {
            handler->PSendSysMessage(
                "Usage: .playerbots gear [player] <white|green|blue|epic|legendary> [max item level]");
            return true;
        }
        uint32 itemQuality = it->second;

        if (!target)
            target = PlayerIdentifier::FromTargetOrSelf(handler);

        if (!target || !target->IsConnected())
        {
            handler->PSendSysMessage("The target character must be online.");
            return true;
        }

        Player* player = target->GetConnectedPlayer();
        if (handler->HasLowerSecurity(player))
            return false;

        // Replaced gear goes to the bags, so a full-ish inventory means some
        // slots keep their old item. Warn up front rather than fail silently.
        uint32 equipped = 0;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (slot != EQUIPMENT_SLOT_BODY && slot != EQUIPMENT_SLOT_TABARD &&
                player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                ++equipped;

        uint32 freeSlots = player->GetFreeInventorySpace();
        if (freeSlots < equipped)
            handler->PSendSysMessage(
                "Warning: {} free bag slot(s) for {} equipped item(s) - slots whose replaced item does not fit "
                "will keep their current gear.",
                freeSlots, equipped);

        uint32 gearScoreLimit =
            maxItemLevel ? PlayerbotFactory::CalcMixedGearScore(*maxItemLevel, itemQuality) : 0;
        PlayerbotFactory factory(player, player->GetLevel(), itemQuality, gearScoreLimit);
        // Exactly the requested tier: no random lowering, no below-tier pool
        // fallback. A slot with no candidates at the tier keeps its gear.
        factory.SetStrictQuality(true);
        factory.InitEquipment(false);
        factory.InitAmmo();
        if (player->GetLevel() >= sPlayerbotAIConfig.minEnchantingBotLevel)
            factory.ApplyEnchantAndGemsNew();
        player->DurabilityRepairAll(false, 1.0f, false);
        player->SaveToDB(false, false);

        handler->PSendSysMessage("Re-geared {} with {} items{} - replaced gear is in their bags.",
                                 handler->playerLink(target->GetName()), qualityName,
                                 maxItemLevel ? Acore::StringFormat(" (max item level {})", *maxItemLevel) : "");
        return true;
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

    // Send an opposing-faction bot on an excursion to the requester's
    // position, picked under the same rules and biases as an organic
    // excursion targeting this zone: level brackets, ganker/home-zone level
    // gaps, category chances, and the stealth-class multipliers. The handler
    // only assigns the payload - the bot's own action performs the guarded
    // teleport and walk-in, so the real path gets exercised end to end.
    static bool HandleWpvpTestCommand(ChatHandler* handler, char const* args)
    {
        Player* requester = handler->GetSession()->GetPlayer();
        if (!requester)
            return false;

        if (!sRandomPlayerbotMgr.IsWpvpExcursionEnabled())
        {
            handler->PSendSysMessage("World PvP excursions are disabled - '.playerbots wpvp on' first.");
            return true;
        }

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

        // The requester's zone plays the role of the destination hub's zone.
        uint32 zoneId = requester->GetZoneId();
        AreaTableEntry const* zone = sAreaTableStore.LookupEntry(zoneId);
        uint32 areaTeam = zone ? zone->team : uint32(AREATEAM_NONE);
        uint32 bracketLow = 0;
        uint32 bracketHigh = 0;
        if (!sTravelMgr.GetZoneLevelBracket(zoneId, bracketLow, bracketHigh))
        {
            handler->PSendSysMessage("This zone has no level bracket - bots never pick it as a wpvp destination.");
            return true;
        }

        if (sPlayerbotAIConfig.IsInPvpProhibitedZone(zoneId))
        {
            handler->PSendSysMessage("This zone is PvP-prohibited - bots never pick it as a wpvp destination.");
            return true;
        }

        struct Candidate
        {
            Player* bot;
            float weight;
            WpvpZoneCategory category;
        };
        std::vector<Candidate> candidates;
        float weightSum = 0.0f;
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

            // Same gates an organic roll applies.
            if (bot->GetLevel() < sPlayerbotAIConfig.wpvpMinBotLevel)
                continue;

            float homeWeight = 0.0f;
            WpvpZoneCategory category =
                ClassifyWpvpDestination(bot, zoneId, areaTeam, bracketLow, bracketHigh, homeWeight);
            if (category == WpvpZoneCategory::None)
                continue;

            // Mirror the organic roll's biases: category chances scaled by
            // the stealth-class overlevel multiplier, the home-zone gap
            // curve, and the stealth-class excursion-start bias.
            bool stealthy = bot->getClass() == CLASS_ROGUE || bot->getClass() == CLASS_DRUID;
            float overlevelMult = stealthy ? sPlayerbotAIConfig.wpvpStealthClassOverlevelMult : 1.0f;
            float weight = 0.0f;
            switch (category)
            {
                case WpvpZoneCategory::Contested:
                    weight = std::max(0.0f, 1.0f - sPlayerbotAIConfig.wpvpHomeZoneChance * overlevelMult -
                                                sPlayerbotAIConfig.wpvpLowerBracketChance * overlevelMult);
                    break;
                case WpvpZoneCategory::LowerBracket:
                    weight = sPlayerbotAIConfig.wpvpLowerBracketChance * overlevelMult;
                    break;
                case WpvpZoneCategory::EnemyHomeZone:
                    weight = sPlayerbotAIConfig.wpvpHomeZoneChance * overlevelMult * homeWeight;
                    break;
                default:
                    break;
            }
            if (stealthy)
                weight *= sPlayerbotAIConfig.wpvpStealthClassWeightMult;

            if (weight <= 0.0f)
                continue;

            candidates.push_back({bot, weight, category});
            weightSum += weight;
        }

        if (candidates.empty())
        {
            handler->PSendSysMessage(
                "No opposing bot passes the wpvp selection rules for this zone (bracket {}-{}, ganker gap {}+, "
                "home-zone gap {}+{}).",
                bracketLow, bracketHigh, sPlayerbotAIConfig.wpvpGankerMinLevelGap,
                sPlayerbotAIConfig.wpvpHomeZoneMinLevelGap, classFilter ? ", class-filtered" : "");
            return true;
        }

        Candidate const* chosen = &candidates.back();
        float pick = frand(0.0f, weightSum);
        float acc = 0.0f;
        for (Candidate const& candidate : candidates)
        {
            acc += candidate.weight;
            if (acc >= pick)
            {
                chosen = &candidate;
                break;
            }
        }

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(chosen->bot);

        NewRpgInfo::GoWpvp payload;
        WorldLocation here(requester->GetMapId(), requester->GetPositionX(), requester->GetPositionY(),
                           requester->GetPositionZ(), requester->GetOrientation());
        if (!ComputeWpvpPositions(here, zoneId, payload))
        {
            handler->PSendSysMessage("Failed to compute excursion positions around you.");
            return true;
        }

        payload.test = true;
        botAI->rpgInfo.ChangeToGoWpvp(std::move(payload));

        char const* categoryName = chosen->category == WpvpZoneCategory::Contested       ? "contested-bracket"
                                   : chosen->category == WpvpZoneCategory::LowerBracket  ? "overleveled ganker"
                                                                                         : "enemy home zone";
        LOG_INFO("playerbots", "[New RPG] wpvp test: sending bot {} (level {} {}, zone {}) to zone {} ({} pick)",
                 chosen->bot->GetName(), chosen->bot->GetLevel(), ChatHelper::FormatClass(chosen->bot->getClass()),
                 chosen->bot->GetZoneId(), zoneId, categoryName);
        handler->PSendSysMessage("Sent {} (level {} {}) on a wpvp excursion to your position ({} pick, {} eligible).",
                                 chosen->bot->GetName(), chosen->bot->GetLevel(),
                                 ChatHelper::FormatClass(chosen->bot->getClass()), categoryName, candidates.size());
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

            EndWpvpExcursion(botAI, "GM kill switch");
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
