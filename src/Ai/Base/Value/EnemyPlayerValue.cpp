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
#include "WpvpGuardRespect.h"
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

        if ((bot->GetTeamId() == TEAM_HORDE && pTarget->HasAura(23333)) ||
            (bot->GetTeamId() == TEAM_ALLIANCE && pTarget->HasAura(23335)))
            return pTarget;

        targets.push_back(pTarget);
    }

    if (!targets.empty())
    {
        std::sort(targets.begin(), targets.end(),
                  [&](Unit const* pUnit1, Unit const* pUnit2)
                  { return bot->GetDistance(pUnit1) < bot->GetDistance(pUnit2); });

        return *targets.begin();
    }

    // 2. Find enemy player in range.

    GuidVector players = AI_VALUE(GuidVector, "nearest enemy players");
    float const maxAggroDistance = GetMaxAttackDistance();
    bool const onWpvpExcursion = botAI->rpgInfo.GetStatus() == RPG_GO_WPVP;
    for (auto const& gTarget : players)
    {
        Unit* pUnit = botAI->GetUnit(gTarget);
        if (!pUnit)
            continue;

        Player* pTarget = dynamic_cast<Player*>(pUnit);
        if (!pTarget)
            continue;

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

        // Aggro weak enemies from further away; excursion bots came to pick fights,
        // so they commit at full range regardless of the health comparison.
        // If controlling mobile vehicle only agro close enemies (otherwise will never reach objective)
        float const aggroDistance = controllingVehicle ? 5.0f
                                    : (controllingCannon || onWpvpExcursion ||
                                       bot->GetHealth() > pTarget->GetHealth())
                                        ? maxAggroDistance
                                        : 20.0f;
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
            // pacifism, and it's off the moment they swing first.
            if (WpvpTruceHolds(bot, pTarget))
            {
                WpvpTruceBoard::instance().NotePassing(bot, pTarget);
                continue;
            }

            return pTarget;
        }
    }

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
                        !WpvpGuardsBarPursuit(bot, pAttacker))
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
