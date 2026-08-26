/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DungeonThreatStrategy.h"

#include "Action.h"
#include "GenericSpellActions.h"
#include "Group.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "ThreatManager.h"
#include "Unit.h"

namespace
{
    // A melee attacker takes a mob off the tank at 110% of the tank's threat, a ranged one at 130%.
    // Backing off at 90% of the tank's threat leaves room for the swing or cast already in flight and
    // for the tick of lag between two evaluations, and uses the melee line for everybody: a bot that
    // is wrong about its own reach is better off a little too quiet than pulling.
    constexpr uint32 DUNGEON_THREAT_SINGLE_PCT = 90;

    // One AoE lands on everything in the pack at once, including the mobs the tank has not reached
    // yet, so the line it has to stay under is the lowest threat lead in the pack and it is drawn
    // lower still - by the time an AoE has taken one mob off the tank it has taken all of them.
    constexpr uint32 DUNGEON_THREAT_AOE_PCT = 60;

    // The main tank this bot has to stay under, or nullptr when there is nothing to stay under: not
    // instanced group content, the bot is the main tank itself, or no main tank can be resolved.
    // Never stored past the call - it is a live pointer.
    Player* GetThreatMainTank(PlayerbotAI* botAI)
    {
        if (!botAI)
            return nullptr;

        Player* bot = botAI->GetBot();
        if (!IsInstancedGroupContent(bot))
            return nullptr;

        ObjectGuid const mainTankGuid = PlayerbotAI::GetMainTankGuid(bot->GetGroup());
        if (mainTankGuid.IsEmpty() || mainTankGuid == bot->GetGUID())
            return nullptr;

        Player* mainTank = ObjectAccessor::FindPlayer(mainTankGuid);
        if (!mainTank || !mainTank->IsInWorld() || !mainTank->IsAlive() || mainTank->GetMap() != bot->GetMap())
            return nullptr;

        return mainTank;
    }

    // This bot is close enough to the main tank's threat on target that more offense would take it.
    bool ExceedsTankThreat(Player* bot, Player* mainTank, Unit* target, uint32 pctOfTank)
    {
        if (!target || !target->IsAlive() || target->IsPlayer() || !target->IsInCombat())
            return false;

        // A mob running for help is nobody's to hold - stopping now only buys it the distance.
        MovementGeneratorType const movement = target->GetMotionMaster()->GetCurrentMovementGeneratorType();
        if (movement == FLEEING_MOTION_TYPE || movement == TIMED_FLEEING_MOTION_TYPE)
            return false;

        // Nothing to stay under: the tank has not touched it. Whether anybody may open on a mob the
        // tank has not picked up is the dungeon hold's call, not this one's.
        float const tankThreat = target->GetThreatMgr().GetThreat(mainTank);
        if (tankThreat <= 0.0f)
            return false;

        float const botThreat = target->GetThreatMgr().GetThreat(bot);
        if (botThreat <= 0.0f)
            return false;

        return botThreat * 100.0f >= tankThreat * static_cast<float>(pctOfTank);
    }
}

void DungeonThreatStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new DungeonThreatMultiplier(botAI));
}

float DungeonThreatMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // The "neglect threat" override the upstream threat strategy reads is deliberately not read
    // here: reading that value clears it, and the only things that set it are raid packs - which
    // are exactly the maps this strategy is never attached to.
    Action::ActionThreatType const threatType = action->getThreatType();
    if (threatType == Action::ActionThreatType::None)
        return 1.0f;

    // Threat type alone does not mean offense. Upstream tags every heal Aoe - a heal does spread
    // threat over everything in combat - and leaves buffs on CastSpellAction's Single default, so
    // throttling by type alone would stop a bot healing or shielding itself exactly at the moment
    // its threat is highest. Auto-attack carries no threat type and keeps running throughout: a
    // throttled caster goes quiet, a throttled melee bot keeps swinging and stops its abilities.
    if (dynamic_cast<CastHealingSpellAction*>(action) || dynamic_cast<CastBuffSpellAction*>(action))
        return 1.0f;

    Player* mainTank = GetThreatMainTank(botAI);
    if (!mainTank)
        return 1.0f;

    if (threatType == Action::ActionThreatType::Aoe)
    {
        GuidVector attackers = AI_VALUE(GuidVector, "attackers");
        for (ObjectGuid const guid : attackers)
        {
            Unit* attacker = botAI->GetUnit(guid);
            if (attacker && ExceedsTankThreat(bot, mainTank, attacker, DUNGEON_THREAT_AOE_PCT))
                return 0.0f;
        }

        return 1.0f;
    }

    // By target rather than by name, the way the dungeon hold suppresses: whatever the action is, it
    // is throttled only when it points at a mob this bot is about to take off the tank. The value
    // can be missing - an action is free to name a target value nothing registers.
    Value<Unit*>* targetValue = action->GetTargetValue();
    if (!targetValue)
        return 1.0f;

    return ExceedsTankThreat(bot, mainTank, targetValue->Get(), DUNGEON_THREAT_SINGLE_PCT) ? 0.0f : 1.0f;
}
