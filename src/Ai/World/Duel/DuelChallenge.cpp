#include "DuelChallenge.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "CellImpl.h"
#include "DBCStores.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Map.h"
#include "NewRpgInfo.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Random.h"
#include "SocialMgr.h"
#include "Timer.h"

namespace
{
constexpr uint32 SPELL_DUEL = 7266;

// The duel challenge spell's own cast range is short; walk in before casting.
constexpr float DUEL_CAST_RANGE = 7.0f;

char const* CHALLENGE_LINES[] = {
    "You. Me. Right now!",
    "Let's see what you've got!",
    "Care for a duel?",
    "I challenge you!",
};

char const* WINNER_LINES[] = {
    "Good fight!",
    "GG! Anyone else?",
    "Well fought!",
};

char const* LOSER_LINES[] = {
    "You got me. Good fight.",
    "gg",
    "I want a rematch sometime!",
};

void BotSay(Player* bot, char const* line)
{
    bot->Say(line, bot->GetTeamId() == TEAM_ALLIANCE ? LANG_COMMON : LANG_ORCISH);
}

bool AreaAllowsDuels(uint32 areaId)
{
    // Mirrors the SPELL_EFFECT_DUEL cast check, so bots don't cast into
    // SPELL_FAILED_NO_DUELING.
    AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(areaId);
    return !areaEntry || (areaEntry->flags & AREA_FLAG_ALLOW_DUELS);
}

// The core allows duels in odd pockets of capital cities where the WMO group
// has no area of its own and the terrain underneath is still painted as the
// surrounding duel-legal zone; keep bots out of anything that resolves to a
// city outright, and let the outdoors/clearance checks handle the pockets.
bool InCapitalCity(Player* player)
{
    AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(player->GetAreaId());
    return areaEntry && (areaEntry->flags & AREA_FLAG_SLAVE_CAPITAL);
}

// No NPC this close to the flag spot - players don't duel on top of vendors.
constexpr float DUEL_NPC_CLEARANCE = 5.0f;

// Ground sampled this far around the flag spot must stay within this height
// band, keeping duels off cliff edges, terraces and steep slopes.
constexpr float DUEL_GROUND_RADIUS = 12.0f;
constexpr float DUEL_GROUND_MAX_DZ = 5.0f;

bool NpcCrowdsSpot(Player* target)
{
    std::list<Creature*> creatures;
    Acore::AnyUnitInObjectRangeCheck check(target, DUEL_NPC_CLEARANCE);
    Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(target, creatures, check);
    Cell::VisitObjects(target, searcher, DUEL_NPC_CLEARANCE);

    for (Creature* creature : creatures)
    {
        if (creature->IsCritter() || creature->IsTrigger() || creature->IsSummon())
            continue;

        return true;
    }

    return false;
}

bool GroundIsFlat(Player* target)
{
    Map* map = target->GetMap();
    float cx = target->GetPositionX();
    float cy = target->GetPositionY();
    float cz = target->GetPositionZ();

    for (uint8 i = 0; i < 8; ++i)
    {
        float dir = float(i) * float(M_PI) / 4.0f;
        float x = cx + std::cos(dir) * DUEL_GROUND_RADIUS;
        float y = cy + std::sin(dir) * DUEL_GROUND_RADIUS;
        float z = map->GetHeight(x, y, cz + 5.0f, true);
        if (z <= INVALID_HEIGHT || std::fabs(z - cz) > DUEL_GROUND_MAX_DZ)
            return false;
    }

    return true;
}
}

bool IsGoodDuelGround(Player* bot, Player* target)
{
    if (!AreaAllowsDuels(bot->GetAreaId()) || !AreaAllowsDuels(target->GetAreaId()))
        return false;

    if (InCapitalCity(bot) || InCapitalCity(target))
        return false;

    if (!bot->IsOutdoors() || !target->IsOutdoors())
        return false;

    // The flag drops at the casting-range midpoint, right next to the target
    // after the walk-in - the target's spot is the proxy for the flag's.
    if (NpcCrowdsSpot(target))
        return false;

    return GroundIsFlat(target);
}

Player* StartDuelPossibleTrigger::FindDuelTarget(PlayerbotAI* botAI, Player* bot)
{
    GuidVector nearPlayers = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest friendly players")->Get();
    float range = float(sPlayerbotAIConfig.duelChallengeRange);

    std::vector<Player*> candidates;
    for (ObjectGuid const guid : nearPlayers)
    {
        Player* player = ObjectAccessor::FindPlayer(guid);
        if (!player || player == bot || !player->IsAlive() || player->duel)
            continue;

        if (player->IsGameMaster() || player->isAFK() || player->IsInCombat() || player->IsBeingTeleported())
            continue;

        if (bot->GetDistance(player) > range)
            continue;

        if (player->GetLevel() > bot->GetLevel() + 3 || bot->GetLevel() > player->GetLevel() + 10)
            continue;

        if (player->GetHealthPct() < 90.0f)
            continue;

        if (!player->GetSocial() || player->GetSocial()->HasIgnore(bot->GetGUID()))
            continue;

        // A mounted real player is on their way somewhere; don't chase them.
        if (!GET_PLAYERBOT_AI(player) && player->IsMounted())
            continue;

        if (!IsGoodDuelGround(bot, player))
            continue;

        candidates.push_back(player);
    }

    if (candidates.empty())
        return nullptr;

    // Prefer someone other than the previous challenge when there's a choice.
    if (candidates.size() > 1)
    {
        ObjectGuid last = botAI->rpgInfo.lastDuelChallengeTarget;
        auto it = std::remove_if(candidates.begin(), candidates.end(),
                                 [last](Player* p) { return p->GetGUID() == last; });
        if (it != candidates.begin())
            candidates.erase(it, candidates.end());
    }

    return candidates[urand(0, candidates.size() - 1)];
}

bool StartDuelPossibleTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.enableBotDuels)
        return false;

    if (!bot->IsAlive() || bot->duel || bot->IsInCombat())
        return false;

    if (bot->GetLevel() < 3)
        return false;

    Map* map = bot->GetMap();
    if (!map || map->IsDungeon() || map->IsBattlegroundOrArena())
        return false;

    // Do not auto duel if the master is around and not AFK.
    if (botAI->HasGameClientMaster() && botAI->GetMaster() && !botAI->GetMaster()->isAFK())
        return false;

    NewRpgInfo& info = botAI->rpgInfo;
    bool atDuelSpot = false;
    if (auto* spot = std::get_if<NewRpgInfo::DuelSpot>(&info.data))
        atDuelSpot = spot->arrivedT != 0;

    uint32 cooldown = (atDuelSpot ? sPlayerbotAIConfig.duelSpotChallengeCooldown
                                  : sPlayerbotAIConfig.duelChallengeCooldown) *
                      IN_MILLISECONDS;
    if (info.lastDuelChallengeT && GetMSTimeDiffToNow(info.lastDuelChallengeT) < cooldown)
        return false;

    // Do not auto duel with low hp.
    if (AI_VALUE2(uint8, "health", "self target") < 90)
        return false;

    // Hostiles around take precedence.
    if (!AI_VALUE(GuidVector, "all targets").empty())
        return false;

    if (sPlayerbotAIConfig.IsInPvpProhibitedZone(bot->GetZoneId()) || !AreaAllowsDuels(bot->GetAreaId()))
        return false;

    // Cheap bot-side location cuts before scanning candidates; the full
    // check (NPC clearance, flat ground) runs per candidate in FindDuelTarget.
    if (InCapitalCity(bot) || !bot->IsOutdoors())
        return false;

    return FindDuelTarget(botAI, bot) != nullptr;
}

bool StartDuelAction::Execute(Event /*event*/)
{
    Player* target = StartDuelPossibleTrigger::FindDuelTarget(botAI, bot);
    if (!target)
        return false;

    if (bot->GetDistance(target) > DUEL_CAST_RANGE)
        return MoveNear(target, DUEL_CAST_RANGE - 2.0f);

    bot->SetFacingToObject(target);

    if (urand(1, 100) <= 40)
        BotSay(bot, CHALLENGE_LINES[urand(0, std::size(CHALLENGE_LINES) - 1)]);

    if (!botAI->CastSpell(SPELL_DUEL, target))
        return false;

    botAI->rpgInfo.lastDuelChallengeT = getMSTime();
    botAI->rpgInfo.lastDuelChallengeTarget = target->GetGUID();
    return true;
}

void OnBotDuelEnded(Player* winner, Player* loser, DuelCompleteType type)
{
    if (type == DUEL_INTERRUPTED)
        return;

    if (winner)
    {
        PlayerbotAI* winnerAI = GET_PLAYERBOT_AI(winner);
        if (winnerAI && winnerAI->IsBotAI())
        {
            if (loser)
                winner->SetFacingToObject(loser);

            if (type == DUEL_FLED)
                winner->HandleEmoteCommand(EMOTE_ONESHOT_POINT);
            else
                winner->HandleEmoteCommand(urand(0, 1) ? EMOTE_ONESHOT_CHEER : EMOTE_ONESHOT_FLEX);

            if (type == DUEL_WON && urand(1, 100) <= 60)
                BotSay(winner, WINNER_LINES[urand(0, std::size(WINNER_LINES) - 1)]);
        }
    }

    if (loser && type == DUEL_WON)
    {
        PlayerbotAI* loserAI = GET_PLAYERBOT_AI(loser);
        if (loserAI && loserAI->IsBotAI())
        {
            loser->HandleEmoteCommand(urand(0, 1) ? EMOTE_ONESHOT_CRY : EMOTE_ONESHOT_SALUTE);

            if (urand(1, 100) <= 40)
                BotSay(loser, LOSER_LINES[urand(0, std::size(LOSER_LINES) - 1)]);
        }
    }
}
