/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EnemyPlayerValue.h"

#include "CombatManager.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "Vehicle.h"
#include "WpvpAssist.h"
#include "WpvpChase.h"
#include "WpvpGrudge.h"
#include "WpvpGuardRespect.h"
#include "WpvpReadiness.h"
#include "WpvpSatiation.h"
#include "WpvpTerrainLos.h"
#include "WpvpTruce.h"

bool NearestEnemyPlayersValue::AcceptUnit(Unit* unit)
{
    // Apply parent's filtering first (includes level difference checks)
    if (!PossibleTargetsValue::AcceptUnit(unit))
        return false;

    bool inCannon = botAI->IsInVehicle(false, true);
    Player* enemy = dynamic_cast<Player*>(unit);
    if (enemy && botAI->IsOpposing(enemy) && enemy->IsPvP() &&
        !sPlayerbotAIConfig.IsPvpProhibited(enemy->GetZoneId(), enemy->GetAreaId()) &&
        !enemy->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NON_ATTACKABLE_2) &&
        ((inCannon || !enemy->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE))) &&
        /*!enemy->HasStealthAura() && !enemy->HasInvisibilityAura()*/ enemy->CanSeeOrDetect(bot) &&
        !(enemy->HasSpiritOfRedemptionAura()))
    {
        // If with master, only attack if master is PvP flagged
        Player* master = botAI->GetMaster();
        if (master && !master->IsPvP() && !master->IsFFAPvP())
            return false;

        // Satiation (Felworld): a bot that just killed this player and
        // rolled "done with them" stops initiating for the grace period -
        // no rez-camping loop. Self-defense is unaffected: the PvP
        // combat-ref path in EnemyPlayerValue::Calculate never comes
        // through here. A revenge grudge overrides a stale satiation from
        // an earlier round of the feud.
        if (WpvpSatiated(bot, enemy) && WpvpGrudgeAgainst(bot, enemy) != WpvpGrudgeDisposition::Revenge)
            return false;

        // Chase leash (Felworld): don't re-acquire a runner we gave up on -
        // the ban clears when they close back in or land a hit.
        if (WpvpChaseBanned(bot, enemy))
            return false;

        return true;
    }

    return false;
}

Unit* EnemyPlayerValue::Calculate()
{
    bool controllingCannon = false;
    bool controllingVehicle = false;
    if (Vehicle* vehicle = bot->GetVehicle())
    {
        VehicleSeatEntry const* seat = vehicle->GetSeatForPassenger(bot);
        if (!seat || !seat->CanControl())  // not in control of vehicle so cant attack anyone
            return nullptr;
        VehicleEntry const* vi = vehicle->GetVehicleInfo();
        if (vi && vi->m_flags & VEHICLE_FLAG_FIXED_POSITION)
            controllingCannon = true;
        else
            controllingVehicle = true;
    }

    // 1. Check units we are currently in PvP combat with.
    std::vector<Unit*> targets;
    Unit* pVictim = bot->GetVictim();
    for (auto const& [guid, combatRef] : bot->GetCombatManager().GetPvPCombatRefs())
    {
        Unit* pTarget = combatRef->GetOther(bot);
        if (!pTarget || pTarget == pVictim || !pTarget->IsPlayer() || !pTarget->CanSeeOrDetect(bot) ||
            !bot->IsWithinDist(pTarget, VISIBILITY_DISTANCE_NORMAL))
            continue;

        // Guard respect (Felworld): an active fight doesn't override the guard
        // bar - an enemy who made it to their outleveling guards gets let go.
        // Without this, the PvP combat ref kept feeding the chase that the
        // validity gate in AttackersValue had already refused.
        if (WpvpGuardsBarPursuit(bot, pTarget))
            continue;

        // Chase leash (Felworld): same for a chase this bot rolled to
        // abandon - the lingering combat ref must not rekindle it.
        if (WpvpChaseBanned(bot, pTarget))
            continue;

        if ((bot->GetTeamId() == TEAM_HORDE && pTarget->HasAura(23333)) ||
            (bot->GetTeamId() == TEAM_ALLIANCE && pTarget->HasAura(23335)))
            return pTarget;

        targets.push_back(pTarget);
    }

    Unit* engaged = nullptr;
    if (!targets.empty())
    {
        std::sort(targets.begin(), targets.end(),
                  [&](Unit const* pUnit1, Unit const* pUnit2)
                  { return bot->GetDistance(pUnit1) < bot->GetDistance(pUnit2); });

        engaged = *targets.begin();

        // Peel (Felworld): in the open world an active fight no longer
        // short-circuits the sighting scan below - a fresh enemy
        // substantially closer than the one we're fighting takes over.
        // Battlegrounds keep the old "nearest active fight wins" behavior.
        if (bot->GetBattleground() || sPlayerbotAIConfig.wpvpPeelAdvantageYards <= 0.0f)
            return engaged;
    }

    // 2. Find enemy player in range.

    GuidVector players = AI_VALUE(GuidVector, "nearest enemy players");

    // Most attractive first: grid iteration order is meaningless, and both
    // the "first acceptable wins" loop and the peel margin below assume the
    // ordering. The score is distance with kill-the-add pull (Felworld): an
    // enemy perceivedly below the bot, or one already fighting players,
    // reads as effectively closer - the underleveled helper who joins a
    // brawl gets focused like an add in a dungeon.
    std::vector<std::pair<float, Player*>> candidates;
    candidates.reserve(players.size());
    for (auto const& gTarget : players)
        if (Player* pTarget = dynamic_cast<Player*>(botAI->GetUnit(gTarget)))
            candidates.push_back({ WpvpTargetScore(bot, pTarget), pTarget });

    std::sort(candidates.begin(), candidates.end(),
              [](auto const& p1, auto const& p2) { return p1.first < p2.first; });

    float const engagedScore = engaged ? WpvpTargetScore(bot, engaged->ToPlayer()) : 0.0f;
    float const maxAggroDistance = GetMaxAttackDistance();
    bool const onWpvpExcursion = botAI->rpgInfo.GetStatus() == RPG_GO_WPVP;
    for (auto const& [score, pTarget] : candidates)
    {
        // Peel margin: while engaged, a bystander only wins when they score
        // WpvpPeelAdvantageYards better than the current fight - enough of a
        // gap that switching reads as opportunism, not indecision. Sorted
        // best-first, so once the margin fails here nobody later beats it.
        if (engaged && score + sPlayerbotAIConfig.wpvpPeelAdvantageYards > engagedScore)
            return engaged;

        if (pTarget == pVictim)
            continue;

        if (bot->GetTeamId() == TEAM_HORDE)
        {
            if (pTarget->HasAura(23333))
                return pTarget;
        }
        else
        {
            if (pTarget->HasAura(23335))
                return pTarget;
        }

        // Initiation readiness (Felworld): an unengaged bot below its
        // health/mana comfort bars passes on the unprovoked pick unless this
        // target's state makes "now" better than "after I'm full up" - see
        // WpvpReadyToInitiate. Self-defense (phase 1) and party assists
        // (phase 3) are never gated.
        if (!engaged && !WpvpReadyToInitiate(bot, pTarget))
        {
            WpvpNoteGatedOpportunity(bot, pTarget);
            continue;
        }

        // Aggro weak enemies from further away; excursion bots came to pick fights,
        // so they commit at full range regardless of the health comparison.
        // If controlling mobile vehicle only agro close enemies (otherwise will never reach objective)
        float aggroDistance = controllingVehicle ? 5.0f
                              : (controllingCannon || onWpvpExcursion ||
                                 bot->GetHealth() > pTarget->GetHealth())
                                  ? maxAggroDistance
                                  : 20.0f;

        // Passerby assist (Felworld): a fight against a faction-mate inside
        // the assist radius is joined even where the health comparison above
        // would have kept the bot at its short range.
        if (!controllingVehicle && WpvpPasserbyAssistTarget(bot, pTarget))
            aggroDistance = std::max(aggroDistance, sPlayerbotAIConfig.wpvpPasserbyAssistRadius);

        // Grudge revenge (Felworld): the bot's recent killer is engaged on
        // sight at full range - the health comparison's 20yd politeness
        // doesn't apply to them.
        if (!controllingVehicle && WpvpGrudgeAgainst(bot, pTarget) == WpvpGrudgeDisposition::Revenge)
            aggroDistance = std::max(aggroDistance, maxAggroDistance);

        if (!bot->IsWithinDist(pTarget, aggroDistance))
            continue;

        // Terrain occlusion (Felworld): the vmap LOS test above sees straight
        // through hills, so also refuse to notice a target the terrain hides.
        if (bot->IsWithinLOSInMap(pTarget) && !WpvpTerrainOccludes(bot, pTarget) &&
            (controllingCannon || (fabs(bot->GetPositionZ() - pTarget->GetPositionZ()) < 30.0f)))
        {
            // Same-class truce (Felworld): this pair honors the old "druids
            // don't gank druids" code - decline the unprovoked attack and
            // queue a salute instead. Only this phase is gated: self-defense
            // (1) and party assists (3) stay - the truce is an offer, not
            // pacifism, and it's off the moment they swing first. Swinging
            // at ANYONE counts: an active combatant forfeits the courtesy.
            if (!WpvpActivePvpCombatant(pTarget) && WpvpTruceHolds(bot, pTarget))
            {
                WpvpTruceBoard::instance().NotePassing(bot, pTarget);
                continue;
            }

            return pTarget;
        }
    }

    if (engaged)
        return engaged;

    // 3. Check party attackers.

    if (Group* pGroup = bot->GetGroup())
    {
        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            if (Unit* pMember = itr->GetSource())
            {
                if (pMember == bot)
                    continue;

                if (ServerFacade::instance().GetDistance2d(bot, pMember) > 30.0f)
                    continue;

                if (Unit* pAttacker = pMember->getAttackerForHelper())
                    if (pAttacker->IsPlayer() && bot->IsWithinDist(pAttacker, maxAggroDistance * 2.0f) &&
                        bot->IsWithinLOSInMap(pAttacker) && !WpvpTerrainOccludes(bot, pAttacker) &&
                        pAttacker != pVictim && pAttacker->CanSeeOrDetect(bot) &&
                        !WpvpGuardsBarPursuit(bot, pAttacker) && !WpvpChaseBanned(bot, pAttacker))
                        return pAttacker;
            }
        }
    }

    return nullptr;
}

float EnemyPlayerValue::GetMaxAttackDistance()
{
    if (!bot->GetBattleground())
        return botAI->rpgInfo.GetStatus() == RPG_GO_WPVP ? sPlayerbotAIConfig.wpvpVisionDistance : 60.0f;

    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return 40.0f;

    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);

    if (bgType == BATTLEGROUND_IC)
    {
        if (botAI->IsInVehicle(false, true))
            return 120.0f;
    }

    return 40.0f;
}
