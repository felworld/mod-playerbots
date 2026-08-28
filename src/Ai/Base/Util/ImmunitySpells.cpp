/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ImmunitySpells.h"

#include "Unit.h"

bool ai::immunity::IsFullyDamageImmune(Unit const* unit)
{
    return unit->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL) &&
           unit->IsImmunedToDamage(SPELL_SCHOOL_MASK_MAGIC);
}

bool ai::immunity::HasDispellableImmunity(Unit const* unit)
{
    if (unit->HasAura(SPELL_DIVINE_SHIELD) || unit->HasAura(SPELL_ICE_BLOCK))
        return true;

    for (uint32 spellId : HandOfProtectionRanks())
        if (unit->HasAura(spellId))
            return true;

    return false;
}
