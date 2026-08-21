/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DungeonPullActions.h"
#include "Event.h"
#include "FelworldEvents.h"
#include "PlayerbotOperations.h"
#include "PlayerbotWorldThreadProcessor.h"
#include "Playerbots.h"
#include "PullActions.h"
#include "PullStrategy.h"
#include "StringFormat.h"

bool DungeonPullAction::isUseful()
{
    if (!sPlayerbotAIConfig.dungeonPullByTank || !IsInstancedGroupContent(bot))
        return false;

    if (!botAI->AllowActivity(GRIND_ACTIVITY))
        return false;

    if (botAI->HasStrategy("stay", BOT_STATE_NON_COMBAT) || botAI->HasStrategy("passive", BOT_STATE_NON_COMBAT))
        return false;

    if (bot->IsInCombat() || !PlayerbotAI::IsMainTank(bot))
        return false;

    if (!IsGroupReady(botAI))
        return false;

    Unit* target = GetTarget();
    return target && target->IsInWorld();
}

bool DungeonPullAction::IsGroupReady(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member)
            continue;

        // Still outside, dead, or fighting: nobody pulls on top of that.
        if (!member->IsInWorld() || member->GetMapId() != bot->GetMapId() || member->IsBeingTeleported())
            return false;

        if (!member->IsAlive() || member->IsInCombat())
            return false;

        // Standing back holds the tank - the on-screen "not yet" a human gives.
        if (member != bot && bot->GetDistance(member) > sPlayerbotAIConfig.dungeonPullGroupRange)
            return false;

        // Sitting is eating or drinking, for humans and bots alike.
        if (member->IsSitState())
            return false;

        if (member->GetHealthPct() < float(sPlayerbotAIConfig.dungeonPullMinHealth))
            return false;

        if (member->getPowerType() == POWER_MANA &&
            member->GetPowerPct(POWER_MANA) < float(sPlayerbotAIConfig.dungeonPullMinMana))
            return false;
    }

    return true;
}

bool DungeonPullAction::Execute(Event event)
{
    Unit* target = GetTarget();
    if (!target || !target->IsInWorld())
        return false;

    // Tank classes carry a PullStrategy (shoot, Icy Touch, Judgement, Faerie Fire); a bot flagged
    // main tank without one, or without the weapon for it, walks in instead.
    PullStrategy* strategy = PullStrategy::Get(botAI);
    bool const ranged = strategy && strategy->CanDoPullAction(target) &&
                        bot->GetDistance(target) <= sPlayerbotAIConfig.reactDistance * 3.0f;

    bool const result = ranged ? PullRequestAction::BeginPull(botAI, target) : AttackAnythingAction::Execute(event);
    if (result)
    {
        LOG_DEBUG("playerbots", "Bot {} pulls {} for the group ({})", bot->GetName(), target->GetName(),
                  ranged ? "ranged" : "body pull");
        Felworld::LogEvent(bot->GetGUID(), "dungeon_pull",
                           Acore::StringFormat("{{\"target\":\"{}\",\"ranged\":{}}}", target->GetName(), ranged));
    }

    return result;
}

bool GiveLeaderToTankAction::isUseful()
{
    if (!sPlayerbotAIConfig.dungeonPullByTank || !IsInstancedGroupContent(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group->IsLeader(bot->GetGUID()))
        return false;

    ObjectGuid const tankGuid = PlayerbotAI::GetMainTankGuid(group);
    if (tankGuid.IsEmpty() || tankGuid == bot->GetGUID())
        return false;

    // With a real player in the group, leadership is theirs ("give leader in dungeon").
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (IsRealPlayer(ref->GetSource()))
            return false;

    Player* tank = ObjectAccessor::FindPlayer(tankGuid);
    return tank && tank->IsInWorld() && GET_PLAYERBOT_AI(tank);
}

bool GiveLeaderToTankAction::Execute(Event /*event*/)
{
    Player* tank = ObjectAccessor::FindPlayer(PlayerbotAI::GetMainTankGuid(bot->GetGroup()));
    if (!tank)
        return false;

    auto setLeaderOp = std::make_unique<GroupSetLeaderOperation>(bot->GetGUID(), tank->GetGUID(),
                                                                 sRandomPlayerbotMgr.IsRandomBot(bot));
    PlayerbotWorldThreadProcessor::instance().QueueOperation(std::move(setLeaderOp));

    botAI->SayToParty(Acore::StringFormat("{}, you lead.", tank->GetName()));
    return true;
}
