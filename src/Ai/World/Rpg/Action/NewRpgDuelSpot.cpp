#include "NewRpgDuelSpot.h"

#include <cmath>
#include <iterator>

#include "AreaDefines.h"
#include "Map.h"
#include "MapMgr.h"
#include "NewRpgWpvp.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Random.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "TravelMgr.h"

namespace
{
// The classic gate duel fields: the graveyard knoll outside Stormwind's
// gates (Elwynn Forest) and the road outside Orgrimmar's front gate
// (Durotar).
WorldLocation const ALLIANCE_DUEL_HUB(0, -9151.98f, 410.94f, 92.70f, 0.0f);
WorldLocation const HORDE_DUEL_HUB(1, 1357.10f, -4412.01f, 28.38f, 0.0f);

char const* SOLICIT_LINES[] = {
    "Anyone up for a duel?",
    "Who thinks they can take me? Duel me!",
    "Looking for a duel! Any takers?",
    "I'll duel anyone here. Step up!",
    "Come on, someone duel me!",
};

constexpr Emote SOLICIT_EMOTES[] = {EMOTE_ONESHOT_FLEX, EMOTE_ONESHOT_ROAR, EMOTE_ONESHOT_POINT};
}

WorldLocation const& GetDuelSpotHub(TeamId team)
{
    return team == TEAM_ALLIANCE ? ALLIANCE_DUEL_HUB : HORDE_DUEL_HUB;
}

bool ComputeDuelSpotPositions(Player* bot, NewRpgInfo::DuelSpot& out)
{
    WorldLocation const& hub = GetDuelSpotHub(bot->GetTeamId());
    Map* map = sMapMgr->FindMap(hub.GetMapId(), 0);
    if (!map)
        return false;

    out.hubPos = WorldPosition(hub.GetMapId(), hub.GetPositionX(), hub.GetPositionY(), hub.GetPositionZ(),
                               hub.GetOrientation());

    WorldPosition anchor;
    bool anchorOk = SampleGroundNear(map, hub, 0.0f, float(M_PI), 8.0f, 30.0f, 15.0f, anchor);
    if (!anchorOk)
        anchor = out.hubPos;

    // Arrive on the anchor's side so the walk-in passes it naturally.
    float bearing = anchorOk ? anchor.GetOrientation() : frand(0.0f, 2.0f * float(M_PI));
    WorldPosition teleport;
    if (!SampleGroundNear(map, hub, bearing, 0.6f, 120.0f, 180.0f, 60.0f, teleport))
        teleport = anchor;

    out.anchorPos = anchor;
    out.teleportPos = teleport;
    return true;
}

void EndDuelSpotHangout(PlayerbotAI* botAI, char const* reason, bool walkIntoCity)
{
    auto* data = std::get_if<NewRpgInfo::DuelSpot>(&botAI->rpgInfo.data);
    if (!data)
        return;

    LOG_DEBUG("playerbots", "[New RPG] Bot {} duel spot hangout ended: {}", botAI->GetBot()->GetName(), reason);

    // Only remove what this hangout added: a bot that rolled "start duel" at
    // AiFactory time keeps challenging while roaming.
    if (data->addedStartDuel)
        botAI->ChangeStrategy("-start duel", BOT_STATE_NON_COMBAT);

    if (walkIntoCity)
    {
        // The capital is right behind the duel field: walk in to the bank
        // district like a player heading home, where the busy-capitals dwell
        // takes over — instead of idling outside the walls, where the zone
        // isn't a capital and the next roll ports the bot somewhere else.
        Player* bot = botAI->GetBot();
        uint32 cityZone = bot->GetTeamId() == TEAM_ALLIANCE ? AREA_STORMWIND_CITY : AREA_ORGRIMMAR;
        if (WorldLocation const* spot = sTravelMgr.GetCapitalBankerLocation(cityZone))
        {
            botAI->rpgInfo.ChangeToGoCamp(WorldPosition(*spot));
            return;
        }
    }

    botAI->rpgInfo.ChangeToIdle();
}

bool NewRpgDuelSpotAction::Execute(Event /*event*/)
{
    auto* data = std::get_if<NewRpgInfo::DuelSpot>(&botAI->rpgInfo.data);
    if (!data)
        return false;

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

bool NewRpgDuelSpotAction::GuardedTeleport(NewRpgInfo::DuelSpot& data)
{
    // Never blink where a real player could watch it happen - at either end.
    WorldPosition telePos = data.teleportPos;
    if (botAI->HasPlayerNearby(150.0f) || RealPlayerNear(telePos, 150.0f))
        return false;

    // Reset(true) wipes rpgInfo (and with it this hangout's payload, which
    // `data` points into), so copy the payload out and restore it after the
    // reset. `data` must not be touched past this point.
    NewRpgInfo::DuelSpot payload = data;
    WorldPosition dest = payload.teleportPos;
    bot->GetMotionMaster()->Clear();
    botAI->Reset(true);
    bot->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
    botAI->rpgInfo.ChangeToDuelSpot(std::move(payload));

    if (!bot->TeleportTo(dest))
        return false;

    bot->SendMovementFlagUpdate();
    if (auto* restored = std::get_if<NewRpgInfo::DuelSpot>(&botAI->rpgInfo.data))
        restored->teleported = true;

    LOG_DEBUG("playerbots", "[New RPG] Bot {} heads to the gate duel spot (map {} {:.1f},{:.1f},{:.1f})",
              bot->GetName(), dest.GetMapId(), dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ());
    return true;
}

bool NewRpgDuelSpotAction::Dwell(NewRpgInfo::DuelSpot& data)
{
    // Re-checked every pass: accepting a duel calls ResetStrategies(), which
    // can drop a strategy added earlier in the dwell.
    if (!botAI->HasStrategy("start duel", BOT_STATE_NON_COMBAT))
    {
        botAI->ChangeStrategy("+start duel", BOT_STATE_NON_COMBAT);
        data.addedStartDuel = true;
    }

    // Duels themselves are the duel/combat machinery's business.
    if (bot->duel || bot->IsInCombat())
        return false;

    if (IsWaitingForLastMove(MovementPriority::MOVEMENT_NORMAL))
        return false;

    // Drifted too far (chased someone, fled): drift back.
    if (bot->GetMapId() != data.anchorPos.GetMapId() || bot->GetExactDist(data.anchorPos) > 120.0f)
        return MoveFarTo(data.anchorPos);

    if (Solicit(data))
        return true;

    // Mostly stand around with the occasional idle wander.
    if (urand(1, 100) <= 35)
        return MoveRandomNear(25.0f);

    return ForceToWait(urand(4000, 8000));
}

bool NewRpgDuelSpotAction::Solicit(NewRpgInfo::DuelSpot& data)
{
    if (data.lastSolicitT &&
        GetMSTimeDiffToNow(data.lastSolicitT) < sPlayerbotAIConfig.duelSpotSolicitCooldown * IN_MILLISECONDS)
        return false;

    // Only worth performing for an audience.
    GuidVector nearPlayers = AI_VALUE(GuidVector, "nearest friendly players");
    std::vector<Player*> audience;
    for (ObjectGuid const guid : nearPlayers)
    {
        Player* player = ObjectAccessor::FindPlayer(guid);
        if (player && player->IsAlive() && !player->duel && !player->IsGameMaster() &&
            bot->GetDistance(player) < 40.0f)
            audience.push_back(player);
    }

    if (audience.empty())
        return false;

    Player* mark = audience[urand(0, audience.size() - 1)];
    bot->SetFacingToObject(mark);
    bot->HandleEmoteCommand(SOLICIT_EMOTES[urand(0, std::size(SOLICIT_EMOTES) - 1)]);

    // DuelChatter=0 keeps the performance wordless (Felworld's llm session
    // mode voices duel spots through mod-llm instead - see mod-llm FEATURES).
    if (sPlayerbotAIConfig.duelChatter && urand(1, 100) <= 60)
        bot->Say(SOLICIT_LINES[urand(0, std::size(SOLICIT_LINES) - 1)],
                 bot->GetTeamId() == TEAM_ALLIANCE ? LANG_COMMON : LANG_ORCISH);

    data.lastSolicitT = getMSTime();
    return true;
}
