/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ImmunityValues.h"

#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SpellAuras.h"
#include "ThreatManager.h"

namespace
{
    // The immunities are instant, so the aura lands within the same second as the record; every
    // one of them has a cooldown far longer than this, so no two casts can both match.
    constexpr time_t CAST_MATCH_WINDOW = 2;
}

bool ImmunityCast::Matches(Aura const* aura) const
{
    if (!aura || !castTime)
        return false;

    time_t const applied = aura->GetApplyTime();
    return applied >= castTime && applied - castTime <= CAST_MATCH_WINDOW;
}

bool SafeToDropImmunityValue::Calculate()
{
    if (!AI_VALUE2(bool, "combat", "self target"))
        return true;

    // The last mob died or the duel ended: nothing is fighting the bot any more, only the core's
    // in-combat flag has yet to run down (up to 5 s). Low health is no reason to wait that out.
    if (FightIsOver())
        return true;

    if (AI_VALUE2(uint8, "health", "self target") < sPlayerbotAIConfig.mediumHealth)
        return false;

    if (botAI->IsTank(bot))
        return true;

    return !MobWouldReturn() && !EnemyPlayerInSight();
}

bool SafeToDropImmunityValue::FightIsOver()
{
    if (bot->duel)
        return false;

    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive())
            return false;
    }

    return !EnemyPlayerInSight();
}

bool SafeToDropImmunityValue::MobWouldReturn()
{
    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        ThreatManager const& threatMgr = unit->GetThreatMgr();
        float const myThreat = threatMgr.GetThreat(bot);
        if (myThreat <= 0.0f)
            continue;

        // Nobody else is holding it, or it holds them less firmly than it does the bot.
        Unit* victim = unit->GetVictim();
        if (!victim || victim == bot || myThreat >= threatMgr.GetThreat(victim))
            return true;
    }

    return false;
}

bool SafeToDropImmunityValue::EnemyPlayerInSight()
{
    GuidVector enemies = AI_VALUE(GuidVector, "nearest enemy players");
    for (ObjectGuid const guid : enemies)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && bot->GetDistance(unit) <= sPlayerbotAIConfig.sightDistance)
            return true;
    }

    return false;
}
