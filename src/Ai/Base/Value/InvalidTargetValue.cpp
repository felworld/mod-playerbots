/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "InvalidTargetValue.h"
#include "AttackersValue.h"
#include "Playerbots.h"
#include "Unit.h"
#include "WpvpChase.h"

bool InvalidTargetValue::Calculate()
{
    Unit* target = AI_VALUE(Unit*, qualifier);

    // Chase leash (Felworld): pursuing an enemy player in the open world
    // rolls periodic "keep chasing?" dice once contact is lost - a broken
    // chase invalidates the target outright, which drops it below.
    if (target && qualifier == "current target" && WpvpChaseBroken(bot, target))
        return true;

    // A target under damage-breakable CC is dropped even when it is the enemy player we are in
    // PvP combat with - otherwise the enemy-player short-circuit below keeps the bot swinging
    // at its own Polymorph/Hex/Sap.
    if (target && qualifier == "current target" && AttackersValue::IsCrowdControlled(target))
        return true;

    Unit* enemy = AI_VALUE(Unit*, "enemy player target");
    if (target && enemy && target == enemy && target->IsAlive())
        return false;

    if (target && qualifier == "current target")
    {
        return target->GetMapId() != bot->GetMapId() || target->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) ||
               target->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) || target->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE_2) ||
               !target->IsVisible() || !target->IsAlive() || target->IsPolymorphed() || target->IsCharmed() ||
               target->HasFearAura() || target->HasUnitState(UNIT_STATE_ISOLATED) || target->IsFriendlyTo(bot) ||
               !AttackersValue::IsValidTarget(target, bot);
    }

    return !target;
}
