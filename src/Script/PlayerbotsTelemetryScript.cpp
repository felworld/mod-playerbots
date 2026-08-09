/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "Battleground.h"
#include "FelworldEvents.h"
#include "Metric.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringFormat.h"
#include "WorldSession.h"

// Lifecycle telemetry for the observability dashboards: deaths, level-ups
// and battleground results. Metrics carry the aggregate story; the
// felworld_events rows put the same moments on each character's inspector
// timeline. Every emission is a no-op while Metric.Enable is off.

namespace
{
char const* FactionTag(Player* player) { return player->GetTeamId() == TEAM_ALLIANCE ? "alliance" : "horde"; }

char const* WhoTag(Player* player) { return player->GetSession()->IsBot() ? "bot" : "player"; }

// Battleground deaths are routine and would drown the world-danger signal,
// so panels split on this tag rather than on zone ids.
char const* ContextTag(Player* player)
{
    if (player->InArena())
        return "arena";
    if (player->InBattleground())
        return "bg";
    return "world";
}

char const* BgTag(Battleground* bg)
{
    switch (bg->GetBgTypeID(true))
    {
        case BATTLEGROUND_AV: return "av";
        case BATTLEGROUND_WS: return "wsg";
        case BATTLEGROUND_AB: return "ab";
        case BATTLEGROUND_EY: return "eots";
        case BATTLEGROUND_SA: return "sota";
        case BATTLEGROUND_IC: return "ioc";
        case BATTLEGROUND_NA:
        case BATTLEGROUND_BE:
        case BATTLEGROUND_RL:
        case BATTLEGROUND_DS:
        case BATTLEGROUND_RV: return "arena";
        default: return "other";
    }
}
}

class PlayerbotsTelemetryPlayerScript : public PlayerScript
{
public:
    PlayerbotsTelemetryPlayerScript()
        : PlayerScript("PlayerbotsTelemetryPlayerScript",
                       { PLAYERHOOK_ON_PLAYER_JUST_DIED, PLAYERHOOK_ON_LEVEL_CHANGED })
    {
    }

    void OnPlayerJustDied(Player* player) override
    {
        METRIC_VALUE("playerbots_deaths", 1, METRIC_TAG("who", WhoTag(player)),
                     METRIC_TAG("faction", FactionTag(player)), METRIC_TAG("context", ContextTag(player)),
                     METRIC_TAG("zone_id", std::to_string(player->GetZoneId())));
        Felworld::LogEvent(player->GetGUID(), "death",
                           Acore::StringFormat("{{\"zone\":{},\"level\":{},\"context\":\"{}\"}}", player->GetZoneId(),
                                               player->GetLevel(), ContextTag(player)));
    }

    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        // Level-downs happen (GM commands, random-bot re-randomization) and
        // are maintenance, not story.
        uint8 newLevel = player->GetLevel();
        if (newLevel <= oldLevel)
            return;

        METRIC_VALUE("playerbots_levelups", 1, METRIC_TAG("who", WhoTag(player)),
                     METRIC_TAG("faction", FactionTag(player)));
        Felworld::LogEvent(player->GetGUID(), "level_up",
                           Acore::StringFormat("{{\"from\":{},\"to\":{}}}", oldLevel, newLevel));
    }
};

class PlayerbotsTelemetryBGScript : public BGScript
{
public:
    PlayerbotsTelemetryBGScript() : BGScript("PlayerbotsTelemetryBGScript") {}

    void OnBattlegroundStart(Battleground* bg) override
    {
        METRIC_VALUE("playerbots_bg", 1, METRIC_TAG("event", "start"), METRIC_TAG("bg", BgTag(bg)));
    }

    void OnBattlegroundEnd(Battleground* bg, TeamId winnerTeamId) override
    {
        char const* winner = winnerTeamId == TEAM_ALLIANCE ? "alliance"
                           : winnerTeamId == TEAM_HORDE    ? "horde"
                                                           : "draw";
        METRIC_VALUE("playerbots_bg", 1, METRIC_TAG("event", "end"), METRIC_TAG("bg", BgTag(bg)),
                     METRIC_TAG("winner", winner));
    }
};

void AddPlayerbotsTelemetryScripts()
{
    new PlayerbotsTelemetryPlayerScript();
    new PlayerbotsTelemetryBGScript();
}
