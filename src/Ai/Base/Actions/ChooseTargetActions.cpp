/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ChooseTargetActions.h"
#include "ChooseRpgTargetAction.h"
#include "CombatManager.h"
#include "Event.h"
#include "LootObjectStack.h"
#include "NewRpgStrategy.h"
#include "Playerbots.h"
#include "PossibleRpgTargetsValue.h"
#include "PvpTriggers.h"
#include "RtiTargetValue.h"
#include "ServerFacade.h"
#include "WpvpVendetta.h"

bool AttackEnemyPlayerAction::Execute(Event event)
{
    // Who threw the first punch matters to the vendetta ledger: attacking
    // an enemy the bot isn't already trading blows with is the bot picking
    // this fight, and losing a fight you picked breeds no resentment.
    Unit* target = GetTarget();
    bool const initiating = target && target->IsPlayer() && !bot->InBattleground() &&
                            !bot->GetCombatManager().GetPvPCombatRefs().count(target->GetGUID());

    bool const result = AttackAction::Execute(event);
    if (result && initiating)
        WpvpVendettaBoard::instance().NoteBotInitiated(bot, target->ToPlayer());

    return result;
}

bool AttackEnemyPlayerAction::isUseful()
{
    if (PlayerHasFlag::IsCapturingFlag(bot))
        return false;

    return !sPlayerbotAIConfig.IsPvpProhibited(bot->GetZoneId(), bot->GetAreaId());
}

bool AttackEnemyFlagCarrierAction::isUseful()
{
    // A bot carrying a flag itself should keep running, not turn to fight
    Unit* target = context->GetValue<Unit*>("enemy flag carrier")->Get();
    return target && ServerFacade::instance().IsDistanceLessOrEqualThan(ServerFacade::instance().GetDistance2d(bot, target), 100.0f) &&
           !PlayerHasFlag::IsCapturingFlag(bot);
}

bool AttackTeamFlagCarrierAttackerAction::isUseful()
{
    return !PlayerHasFlag::IsCapturingFlag(bot) && context->GetValue<Unit*>("team fc attacker")->Get();
}

bool AggressiveTargetAction::isUseful()
{
    if (bot->IsInCombat())
        return false;

    return true;
}

bool DropTargetAction::Execute(Event /*event*/)
{
    Unit* target = context->GetValue<Unit*>("current target")->Get();
    if (target && target->isDead())
    {
        ObjectGuid guid = target->GetGUID();
        if (guid)
            context->GetValue<LootObjectStack*>("available loot")->Get()->Add(guid);
    }

    // ObjectGuid pullTarget = context->GetValue<ObjectGuid>("pull target")->Get();
    // GuidVector possible = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();

    // if (pullTarget && find(possible.begin(), possible.end(), pullTarget) == possible.end())
    // {
    //     context->GetValue<ObjectGuid>("pull target")->Set(ObjectGuid::Empty);
    // }

    context->GetValue<Unit*>("current target")->Set(nullptr);

    bot->SetTarget(ObjectGuid::Empty);
    bot->SetSelection(ObjectGuid());
    botAI->ChangeEngine(BOT_STATE_NON_COMBAT);
    if (bot->getClass() == CLASS_HUNTER) // Check for Hunter Class
    {
        Spell const* spell = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL); // Get the current spell being cast by the bot
        if (spell && spell->m_spellInfo->Id == 75) //Check spell is not nullptr before accessing m_spellInfo
            bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL); // Interrupt Auto Shot
    }
    bot->AttackStop();

    // The bot just disengaged, but core PetAI keeps the pet on whatever last hit it - in a dungeon
    // that walks the pet (and its aggro) into the next pack. Recall it to the owner instead; the
    // "pet attack" trigger re-commands it as soon as the bot picks a new target.
    // Note: the original version of this recall (disabled in 25da0af7) also forced REACT_PASSIVE and
    // never restored it, which left pets permanently inert. Stance is deliberately untouched here,
    // so a defensive pet still defends its owner while following.
    if (Pet* pet = bot->GetPet())
    {
        if (pet->GetVictim())
            botAI->PetFollow();
    }

    return true;
}

bool AttackAnythingAction::Execute(Event event)
{
    bool result = AttackAction::Execute(event);
    if (result)
    {
        if (Unit* grindTarget = GetTarget())
        {
            context->GetValue<ObjectGuid>("pull target")->Set(grindTarget->GetGUID());
            bot->GetMotionMaster()->Clear();
            // bot->StopMoving();
        }
    }

    return result;
}

bool AttackAnythingAction::isUseful()
{
    if (!bot || !botAI)  // Prevents invalid accesses
        return false;

    if (!botAI->AllowActivity(GRIND_ACTIVITY))  // Bot cannot be active
        return false;

    if (botAI->HasStrategy("stay", BOT_STATE_NON_COMBAT))
        return false;

    if (bot->IsInCombat())
        return false;

    // While a nearby enemy player sits behind a full damage immunity, starting a mob fight hands
    // them a free opener the moment it drops - hold the pulls until the bubble runs out or they
    // leave. Trigger priority alone doesn't stop this: the standoff winning the tick doesn't keep
    // lower-relevance actions from also running in it (Felworld).
    if (AI_VALUE(Unit*, "immune enemy near"))
        return false;

    // In instanced group content the opener is the main tank (the "dungeon pull" strategy), not
    // whichever bot happens to hold group leadership - and never a bot when the tank is a real
    // player (Felworld). Without any tank in the group the leader keeps grinding as upstream.
    if (sPlayerbotAIConfig.dungeonPullByTank && IsInstancedGroupContent(bot) &&
        !PlayerbotAI::GetMainTankGuid(bot->GetGroup()).IsEmpty())
        return false;

    Unit* target = GetTarget();
    if (!target || !target->IsInWorld())  // Checks if the target is valid and in the world
        return false;

    std::string const name = std::string(target->GetName());
    if (!name.empty() &&
        (name.find("Dummy") != std::string::npos ||
         name.find("Charge Target") != std::string::npos ||
         name.find("Melee Target") != std::string::npos ||
         name.find("Ranged Target") != std::string::npos))
    {
        return false;
    }

    return true;
}

bool AttackAnythingAction::isPossible() { return GetTarget() && AttackAction::isPossible(); }

bool DpsAssistAction::isUseful()
{
    if (PlayerHasFlag::IsCapturingFlag(bot))
        return false;

    return true;
}

bool AttackRtiTargetAction::Execute(Event /*event*/)
{
    Unit* rtiTarget = AI_VALUE(Unit*, "rti target");

    // Fallback: if the "rti target" value did not resolve a valid unit yet,
    // try to resolve the raid icon directly from the group.
    if (!rtiTarget)
    {
        if (Group* group = bot->GetGroup())
        {
            std::string const rti = AI_VALUE(std::string, "rti");
            int32 const index = RtiTargetValue::GetRtiIndex(rti);
            if (index >= 0)
            {
                ObjectGuid const guid = group->GetTargetIcon(index);
                if (!guid.IsEmpty())
                    rtiTarget = botAI->GetUnit(guid);
            }
        }
    }

    if (rtiTarget && rtiTarget->IsInWorld() && rtiTarget->GetMapId() == bot->GetMapId())
    {
        botAI->GetAiObjectContext()->GetValue<GuidVector>("prioritized targets")->Set({rtiTarget->GetGUID()});
        bool result = Attack(botAI->GetUnit(rtiTarget->GetGUID()));
        if (result)
        {
            context->GetValue<ObjectGuid>("pull target")->Set(rtiTarget->GetGUID());
            return true;
        }
    }
    else
        botAI->TellError("I dont see my rti attack target");

    return false;
}

bool AttackRtiTargetAction::isUseful()
{
    if (botAI->ContainsStrategy(STRATEGY_TYPE_HEAL))
        return false;

    return true;
}
