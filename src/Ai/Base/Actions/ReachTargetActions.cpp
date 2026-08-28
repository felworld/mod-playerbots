/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ReachTargetActions.h"
#include "Event.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ServerFacade.h"

bool ReachTargetAction::Execute(Event /*event*/) { return ReachCombatTo(AI_VALUE(Unit*, GetTargetName()), distance); }

bool ReachTargetAction::isUseful()
{
    // do not move while staying
    if (botAI->HasStrategy("stay", botAI->GetState()))
    {
        return false;
    }

    // do not move while casting
    if (bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr)
    {
        return false;
    }
    Unit* target = GetTarget();
    // float dis = distance + CONTACT_DISTANCE;
    return target &&
           !bot->IsWithinCombatRange(target, distance);  // ServerFacade::instance().IsDistanceGreaterThan(AI_VALUE2(float,
                                                         // "distance", GetTargetName()), distance);
}

std::string const ReachTargetAction::GetTargetName() { return "current target"; }

bool CastReachTargetSpellAction::isUseful()
{
    // do not move while staying
    if (botAI->HasStrategy("stay", botAI->GetState()))
    {
        return false;
    }

    return ServerFacade::instance().IsDistanceGreaterThan(AI_VALUE2(float, "distance", "current target"),
                                                (distance + sPlayerbotAIConfig.contactDistance));
}

ReachSpellAction::ReachSpellAction(PlayerbotAI* botAI)
    : ReachTargetAction(botAI, "reach spell", botAI->GetRange("spell"))
{
}

ReachPartyMemberToHealAction::ReachPartyMemberToHealAction(PlayerbotAI* botAI)
    : ReachTargetAction(botAI, "reach party member to heal", botAI->GetRange("heal"))
{
}

std::string const ReachPartyMemberToHealAction::GetTargetName() { return "party member to heal"; }

ReachPartyMemberToResurrectAction::ReachPartyMemberToResurrectAction(PlayerbotAI* botAI)
    : ReachTargetAction(botAI, "reach party member to resurrect", botAI->GetRange("spell"))
{
}

std::string const ReachPartyMemberToResurrectAction::GetTargetName() { return "party member to resurrect"; }

ReachPartyMemberToJumperCableAction::ReachPartyMemberToJumperCableAction(PlayerbotAI* botAI)
    : ReachTargetAction(botAI, "reach party member to jumper cable", INTERACTION_DISTANCE / 2)
{
}

bool ReachPartyMemberToJumperCableAction::isUseful()
{
    if (botAI->HasStrategy("stay", botAI->GetState()) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        return false;

    // The base check pads `distance` with both units' combat reach, which for a
    // large-model bot can declare "reached" while still outside item interaction
    // range and strand the walk; the cables action needs the raw distance.
    Unit* target = GetTarget();
    return target && !bot->IsWithinDistInMap(target, INTERACTION_DISTANCE);
}

std::string const ReachPartyMemberToJumperCableAction::GetTargetName() { return "party member to jumper cable"; }
