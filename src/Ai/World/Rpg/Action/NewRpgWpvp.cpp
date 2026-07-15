#include "NewRpgWpvp.h"

#include <cmath>

#include "DBCStores.h"
#include "Map.h"
#include "MapMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Random.h"
#include "SharedDefines.h"
#include "Timer.h"

namespace
{
// Sample a ground position distMin..distMax yards from the hub at
// bearing +/- bearingSpread, rejecting spots whose ground height differs
// from the hub's by more than zTolerance (roofs, cliffs, cellars).
bool SampleGroundNear(Map* map, WorldLocation const& hub, float bearing, float bearingSpread, float distMin,
                      float distMax, float zTolerance, WorldPosition& out)
{
    for (uint32 i = 0; i < 10; ++i)
    {
        float dir = bearing + frand(-bearingSpread, bearingSpread);
        float dist = frand(distMin, distMax);
        float x = hub.GetPositionX() + std::cos(dir) * dist;
        float y = hub.GetPositionY() + std::sin(dir) * dist;
        float z = map->GetHeight(x, y, hub.GetPositionZ() + 30.0f, true);
        if (z <= INVALID_HEIGHT || std::fabs(z - hub.GetPositionZ()) > zTolerance)
            continue;

        out = WorldPosition(hub.GetMapId(), x, y, z + 0.5f, dir);
        return true;
    }
    return false;
}
}  // namespace

bool ComputeWpvpPositions(WorldLocation const& hubLoc, uint32 zoneId, NewRpgInfo::GoWpvp& out)
{
    Map* map = sMapMgr->FindMap(hubLoc.GetMapId(), 0);
    if (!map)
        return false;

    out.hubPos = WorldPosition(hubLoc.GetMapId(), hubLoc.GetPositionX(), hubLoc.GetPositionY(),
                               hubLoc.GetPositionZ(), hubLoc.GetOrientation());
    out.zoneId = zoneId;

    WorldPosition anchor;
    bool anchorOk = SampleGroundNear(map, hubLoc, 0.0f, float(M_PI), sPlayerbotAIConfig.wpvpAnchorOffsetMin,
                                     sPlayerbotAIConfig.wpvpAnchorOffsetMax, 20.0f, anchor);
    if (!anchorOk)
        anchor = out.hubPos;

    // Approach from the anchor's side of town so the walk-in passes the
    // anchor naturally instead of cutting through the hub.
    float bearing = anchorOk ? anchor.GetOrientation() : frand(0.0f, 2.0f * float(M_PI));
    WorldPosition teleport;
    if (!SampleGroundNear(map, hubLoc, bearing, 0.6f, sPlayerbotAIConfig.wpvpTeleportOffsetMin,
                          sPlayerbotAIConfig.wpvpTeleportOffsetMax, 60.0f, teleport))
        teleport = anchor;

    out.anchorPos = anchor;
    out.teleportPos = teleport;
    return true;
}

WpvpZoneCategory ClassifyWpvpDestination(Player* invader, uint32 zoneId, uint32 areaTeam, uint32 bracketLow,
                                         uint32 bracketHigh, float& homeWeight)
{
    homeWeight = 0.0f;
    if (sPlayerbotAIConfig.IsInPvpProhibitedZone(zoneId) || zoneId == invader->GetZoneId())
        return WpvpZoneCategory::None;

    uint32 level = invader->GetLevel();
    if (areaTeam == AREATEAM_NONE)
    {
        if (level >= bracketLow && level <= bracketHigh)
            return WpvpZoneCategory::Contested;

        if (level >= bracketHigh + sPlayerbotAIConfig.wpvpGankerMinLevelGap)
            return WpvpZoneCategory::LowerBracket;

        return WpvpZoneCategory::None;
    }

    // A faction-owned zone is only a destination when it's the ENEMY's, and
    // only for clearly overleveled bots. The weight ramps from 25% at the
    // minimum gap to 100% at the full-chance gap so mid-level gankers show
    // up in low zones about as often as max-level ones.
    uint32 enemyAreaTeam = invader->GetTeamId() == TEAM_ALLIANCE ? AREATEAM_HORDE : AREATEAM_ALLY;
    if (areaTeam != enemyAreaTeam)
        return WpvpZoneCategory::None;

    uint32 minReq = bracketHigh + sPlayerbotAIConfig.wpvpHomeZoneMinLevelGap;
    if (level < minReq)
        return WpvpZoneCategory::None;

    float gapRange =
        float(sPlayerbotAIConfig.wpvpHomeZoneFullChanceGap) - float(sPlayerbotAIConfig.wpvpHomeZoneMinLevelGap);
    float progress = gapRange > 0.0f ? std::min(1.0f, float(level - minReq) / gapRange) : 1.0f;
    homeWeight = 0.25f + 0.75f * progress;
    return WpvpZoneCategory::EnemyHomeZone;
}

void EndWpvpExcursion(PlayerbotAI* botAI)
{
    auto* data = std::get_if<NewRpgInfo::GoWpvp>(&botAI->rpgInfo.data);
    if (!data)
        return;

    // Only remove what this excursion added: a bot that brought stealth from
    // elsewhere (e.g. a BG strategy set) keeps it.
    if (data->strategiesApplied)
    {
        Player* bot = botAI->GetBot();
        if (bot->getClass() == CLASS_ROGUE)
            botAI->ChangeStrategy("-stealth", BOT_STATE_NON_COMBAT);
        else if (bot->getClass() == CLASS_DRUID)
            botAI->ChangeStrategy("-prowl", BOT_STATE_NON_COMBAT);
        botAI->ChangeStrategy("-wpvp", BOT_STATE_NON_COMBAT);
    }

    botAI->rpgInfo.ChangeToIdle();
}

bool NewRpgGoWpvpAction::Execute(Event /*event*/)
{
    auto* data = std::get_if<NewRpgInfo::GoWpvp>(&botAI->rpgInfo.data);
    if (!data)
        return false;

    // Flag up front so arrival in contested territory isn't a surprise flip
    // mid-fight; on this excursion the bot is fair game the whole way.
    if (!bot->IsPvP())
        bot->SetPvP(true);

    if (!data->teleported)
        return GuardedTeleport(*data);

    if (!data->arrivedT)
    {
        if (MoveFarTo(data->anchorPos))
            return true;
        // Small nudge so the next tick's MoveFarTo starts from a slightly
        // different position (same trick as NewRpgGoGrindAction).
        return MoveRandomNear(10.0f);
    }

    return Dwell(*data);
}

bool NewRpgGoWpvpAction::GuardedTeleport(NewRpgInfo::GoWpvp& data)
{
    // Never blink where a real player could watch it happen - at either end.
    if (botAI->HasPlayerNearby(150.0f))
        return false;
    WorldPosition telePos = data.teleportPos;
    if (botAI->HasPlayerNearby(&telePos, 150.0f))
        return false;

    // Reset(true) wipes rpgInfo (and with it this excursion's payload, which
    // `data` points into), so copy the payload out and restore it after the
    // reset. `data` must not be touched past this point.
    NewRpgInfo::GoWpvp payload = data;
    uint32 zoneId = payload.zoneId;
    WorldPosition dest = payload.teleportPos;
    bot->GetMotionMaster()->Clear();
    botAI->Reset(true);
    bot->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
    botAI->rpgInfo.ChangeToGoWpvp(std::move(payload));

    if (!bot->TeleportTo(dest))
        return false;

    bot->SendMovementFlagUpdate();
    bool test = false;
    if (auto* restored = std::get_if<NewRpgInfo::GoWpvp>(&botAI->rpgInfo.data))
    {
        restored->teleported = true;
        test = restored->test;
    }

    // Test excursions log at INFO so the port-in is guaranteed to reach the
    // worldserver console (whose appender typically caps at INFO).
    if (test)
        LOG_INFO("playerbots",
                 "[New RPG] Bot {} (wpvp test) ported in to zone {} (map {} {:.1f},{:.1f},{:.1f}); walking in",
                 bot->GetName(), zoneId, dest.GetMapId(), dest.GetPositionX(), dest.GetPositionY(),
                 dest.GetPositionZ());
    else
        LOG_DEBUG("playerbots",
                  "[New RPG] Bot {} starts wpvp excursion to zone {} (arrival map {} {:.1f},{:.1f},{:.1f})",
                  bot->GetName(), zoneId, dest.GetMapId(), dest.GetPositionX(), dest.GetPositionY(),
                  dest.GetPositionZ());
    return true;
}

bool NewRpgGoWpvpAction::Dwell(NewRpgInfo::GoWpvp& data)
{
    if (!data.strategiesApplied)
    {
        if (bot->getClass() == CLASS_ROGUE)
            botAI->ChangeStrategy("+stealth", BOT_STATE_NON_COMBAT);
        else if (bot->getClass() == CLASS_DRUID)
            botAI->ChangeStrategy("+prowl", BOT_STATE_NON_COMBAT);
        botAI->ChangeStrategy("+wpvp", BOT_STATE_NON_COMBAT);
        data.strategiesApplied = true;
    }

    // Fights themselves are the combat engine's business.
    if (bot->IsInCombat())
        return false;

    if (IsWaitingForLastMove(MovementPriority::MOVEMENT_NORMAL))
        return false;

    // Drifted too far (chased someone, fled a guard): drift back.
    if (bot->GetMapId() != data.anchorPos.GetMapId() || bot->GetExactDist(data.anchorPos) > 120.0f)
        return MoveFarTo(data.anchorPos);

    // Mostly stand around - standing still is what lets Shadowmeld hold and
    // stealthers pick their moment - with the occasional idle wander.
    if (urand(1, 100) <= 35)
        return MoveRandomNear(30.0f);

    return ForceToWait(urand(4000, 8000));
}
