/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TargetValue.h"
#include "AttackersValue.h"
#include "CombatManager.h"
#include "Creature.h"
#include "LastMovementValue.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectGuid.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include "ScriptedCreature.h"
#include "Strategy.h"
#include "ThreatManager.h"

namespace
{
    // Past this the tank is off fighting something the bot cannot reasonably join - the next pack, a
    // runner it chased down - so the bot scores locally instead of trailing after it (Felworld).
    constexpr float TANK_ASSIST_MAX_DISTANCE = 40.0f;
}

GuidSet GatherStrategyTargetExclusions(PlayerbotAI* botAI, TargetValueExclusionType type)
{
    GuidSet exclusions;
    if (!botAI || type == TargetValueExclusionType::None || !botAI->HasTargetExclusions())
        return exclusions;

    for (auto const& strategyName : botAI->GetStrategies(BOT_STATE_COMBAT))
    {
        Strategy* strategy = botAI->GetStrategy(strategyName, BOT_STATE_COMBAT);
        if (!strategy)
            continue;

        strategy->AppendTargetExclusions(exclusions, type);
    }

    return exclusions;
}

bool IsFleeingForAssistance(Unit* unit)
{
    if (!unit || !unit->IsCreature())
        return false;

    switch (unit->GetMotionMaster()->GetCurrentMovementGeneratorType())
    {
        // Running to the assist creature, then the short pause once it gets there.
        case ASSISTANCE_MOTION_TYPE:
        case ASSISTANCE_DISTRACT_MOTION_TYPE:
        // No assist creature was in range, so the mob just runs for a fixed time instead. Fear auras
        // never land here - they pass isFear, which yields an untimed FLEEING_MOTION_TYPE.
        case TIMED_FLEEING_MOTION_TYPE:
            return true;
        default:
            return false;
    }
}

bool IsFleeingFromCombat(Unit* unit)
{
    if (!unit)
        return false;

    if (unit->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLEEING_MOTION_TYPE)
        return true;

    return IsFleeingForAssistance(unit);
}

Unit* FindTargetStrategy::GetResult() { return result; }

TargetValueExclusionType FindTargetStrategy::GetExclusionType() { return TargetValueExclusionType::None; }

Unit* TargetValue::FindTarget(FindTargetStrategy* strategy)
{
    GuidVector attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    GuidSet const dynamicExclusions = GatherStrategyTargetExclusions(botAI, strategy->GetExclusionType());
    bool const skipCrowdControlled = strategy->IsDpsSearch();
    GuidVector controlled;

    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || dynamicExclusions.find(guid) != dynamicExclusions.end())
            continue;

        // "attackers" is recomputed once a second and lets marked units through unfiltered, so a mob
        // sheeped a moment ago is still on the list (Felworld). Re-checking live is what keeps a whole
        // row of damage dealers from retargeting onto a fresh Polymorph and starting the re-sheep loop.
        if (skipCrowdControlled && AttackersValue::IsCrowdControlled(unit))
        {
            controlled.push_back(guid);
            continue;
        }

        ThreatManager& threatMgr = unit->GetThreatMgr();
        strategy->CheckAttacker(unit, &threatMgr);
    }

    if (Unit* result = strategy->GetResult())
        return result;

    // Nothing else is left standing: breaking the control beats standing around waiting for it to fade.
    for (ObjectGuid const guid : controlled)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        ThreatManager& threatMgr = unit->GetThreatMgr();
        strategy->CheckAttacker(unit, &threatMgr);
    }

    return strategy->GetResult();
}

bool FindNonCcTargetStrategy::IsCcTarget(Unit* attacker)
{
    if (Group* group = botAI->GetBot()->GetGroup())
    {
        Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
        for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
        {
            Player* member = ObjectAccessor::FindPlayer(itr->guid);
            if (!member || !member->IsAlive())
                continue;

            if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(member))
            {
                if (botAI->GetAiObjectContext()->GetValue<Unit*>("rti cc target")->Get() == attacker)
                    return true;

                std::string const rti = botAI->GetAiObjectContext()->GetValue<std::string>("rti cc")->Get();
                int32 index = RtiTargetValue::GetRtiIndex(rti);
                if (index != -1)
                {
                    if (ObjectGuid guid = group->GetTargetIcon(index))
                        if (attacker->GetGUID() == guid)
                            return true;
                }
            }
        }

        if (ObjectGuid guid = group->GetTargetIcon(4))
            if (attacker->GetGUID() == guid)
                return true;
    }

    return false;
}

void FindTargetStrategy::GetPlayerCount(Unit* creature, uint32* tankCount, uint32* dpsCount)
{
    Player* bot = botAI->GetBot();
    if (tankCountCache.find(creature) != tankCountCache.end())
    {
        *tankCount = tankCountCache[creature];
        *dpsCount = dpsCountCache[creature];
        return;
    }

    *tankCount = 0;
    *dpsCount = 0;

    Unit::AttackerSet attackers(creature->getAttackers());
    for (Unit* attacker : attackers)
    {
        if (!attacker || !attacker->IsAlive() || attacker == bot)
            continue;

        Player* player = attacker->ToPlayer();
        if (!player)
            continue;

        if (botAI->IsTank(player))
            ++(*tankCount);
        else
            ++(*dpsCount);
    }

    tankCountCache[creature] = *tankCount;
    dpsCountCache[creature] = *dpsCount;
}

TargetPriority FindTargetStrategy::GetPriority(Unit* attacker)
{
    Player* bot = botAI->GetBot();

    if (Group* group = bot->GetGroup())
    {
        ObjectGuid guid = group->GetTargetIcon(7);
        if (guid && attacker->GetGUID() == guid)
        {
            return TargetPriority::Marked;
        }
    }
    GuidVector prioritizedTargets = botAI->GetAiObjectContext()->GetValue<GuidVector>("prioritized targets")->Get();
    for (ObjectGuid targetGuid : prioritizedTargets)
    {
        if (targetGuid && attacker->GetGUID() == targetGuid)
        {
            return TargetPriority::Marked;
        }
    }

    // Outside battlegrounds, any enemy player among our attackers is a world-PvP
    // assailant — they outrank whatever mob we were already fighting.
    if (attacker->IsPlayer() && !bot->InBattleground())
        return TargetPriority::Marked;

    // Kill the runner before it drags the rest of the room back. Instances only: out in the world a
    // fleeing mob is simply leaving, and chasing it is how bots pull half a zone. Bosses are left to
    // their scripts — a scripted retreat is not an add pull and must not override a mark.
    if (IsFleeingForAssistance(attacker))
    {
        Creature* creature = attacker->ToCreature();
        Map* map = bot->GetMap();
        if (map && map->IsDungeon() && !creature->isWorldBoss() && !creature->IsDungeonBoss())
            return TargetPriority::FleeingForAssistance;
    }

    // Assist the tank (Felworld): whatever it is swinging at is the group's target, and the bot moves
    // with it when it switches. Ranked below a runner, which is still worth peeling off for, and below
    // a mark, which is the group saying otherwise out loud.
    if (IsDpsSearch() && attacker->GetGUID() == GetTankAssistTarget())
        return TargetPriority::AssistTank;

    return TargetPriority::Normal;
}

bool FindTargetStrategy::IsHighPriority(Unit* attacker) { return GetPriority(attacker) != TargetPriority::Normal; }

bool FindTargetStrategy::IsDpsSearch() { return GetExclusionType() == TargetValueExclusionType::Dps; }

ObjectGuid const& FindTargetStrategy::GetTankAssistTarget()
{
    if (tankAssistResolved)
        return tankAssistTarget;

    tankAssistResolved = true;

    Player* bot = botAI->GetBot();

    // Instanced group content only. Out in the world there is no pull order to hold to, and funnelling
    // every bot onto one mob would turn open-world fights into a single-minded blob.
    if (!IsInstancedGroupContent(bot))
        return tankAssistTarget;

    ObjectGuid const tankGuid = PlayerbotAI::GetMainTankGuid(bot->GetGroup());
    if (tankGuid.IsEmpty() || tankGuid == bot->GetGUID())
        return tankAssistTarget;

    Player* tank = ObjectAccessor::FindPlayer(tankGuid);
    if (!tank || !tank->IsInWorld() || !tank->IsAlive() || tank->GetMap() != bot->GetMap())
        return tankAssistTarget;

    // Falling through to the normal scoring covers every "the tank has nothing usable" case: between
    // pulls, dead or despawned victim, a victim this bot may not hit, and one just crowd-controlled.
    Unit* victim = tank->GetVictim();
    if (!victim || !victim->IsAlive() || !bot->IsValidAttackTarget(victim) ||
        AttackersValue::IsCrowdControlled(victim))
        return tankAssistTarget;

    if (bot->GetDistance(victim) > TANK_ASSIST_MAX_DISTANCE)
        return tankAssistTarget;

    tankAssistTarget = victim->GetGUID();
    return tankAssistTarget;
}

bool FindTargetStrategy::CheckPriority(Unit* attacker)
{
    if (highestPriority == TargetPriority::Marked)
        return true;

    TargetPriority const priority = GetPriority(attacker);
    if (priority > highestPriority)
    {
        highestPriority = priority;
        result = attacker;
    }

    return highestPriority != TargetPriority::Normal;
}

WorldPosition LastLongMoveValue::Calculate()
{
    LastMovement& lastMove = *context->GetValue<LastMovement&>("last movement");
    if (lastMove.lastPath.empty())
        return WorldPosition();

    return lastMove.lastPath.getBack();
}

WorldPosition HomeBindValue::Calculate()
{
    return WorldPosition(bot->m_homebindMapId, bot->m_homebindX, bot->m_homebindY, bot->m_homebindZ, 0.f);
}

Unit* FindTargetValue::Calculate()
{
    if (qualifier == "")
    {
        return nullptr;
    }
    Group* group = bot->GetGroup();
    if (!group)
    {
        return nullptr;
    }
    for (auto const& [guid, ref] : bot->GetThreatMgr().GetThreatenedByMeList())
    {
        Unit* unit = ref->GetOwner();
        if (!unit)
            continue;

        std::wstring wnamepart;
        Utf8toWStr(unit->GetName(), wnamepart);
        wstrToLower(wnamepart);
        if (!qualifier.empty() && qualifier.length() == wnamepart.length() && Utf8FitTo(qualifier, wnamepart))
            return unit;
    }

    return nullptr;
}

void FindBossTargetStrategy::CheckAttacker(Unit* attacker, ThreatManager* /*threatManager*/)
{
    UnitAI* unitAI = attacker->GetAI();
    BossAI* bossAI = dynamic_cast<BossAI*>(unitAI);
    if (bossAI)
    {
        result = attacker;
    }
}

Unit* BossTargetValue::Calculate()
{
    FindBossTargetStrategy strategy(botAI);
    return FindTarget(&strategy);
}
