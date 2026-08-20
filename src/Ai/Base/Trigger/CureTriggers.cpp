/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "CureTriggers.h"
#include "Playerbots.h"
#include "WorldBuffAction.h"

bool NeedCureTrigger::IsActive()
{
    Unit* target = GetTarget();
    return target && target->IsInWorld() && botAI->HasAuraToDispel(target, dispelType);
}

bool TargetAuraDispelTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || bot->IsFriendlyTo(target))
        return false;

    // An offensive dispel costs a GCD and mana that the damage kit needs more when running dry
    if (bot->getPowerType() == POWER_MANA)
    {
        uint32 maxMana = bot->GetMaxPower(POWER_MANA);
        if (!maxMana || bot->GetPower(POWER_MANA) * 100 / maxMana < 40)
            return false;
    }

    return NeedCureTrigger::IsActive();
}

Value<Unit*>* PartyMemberNeedCureTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("party member to dispel", dispelType);
}

bool PartyMemberNeedCureTrigger::IsActive()
{
    Unit* target = GetTarget();
    return target && target->IsInWorld();
}

bool NeedWorldBuffTrigger::IsActive() { return !WorldBuffAction::NeedWorldBuffs(bot).empty(); }
