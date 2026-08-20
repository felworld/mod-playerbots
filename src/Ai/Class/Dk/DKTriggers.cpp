/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DKTriggers.h"
#include "AttackersValue.h"
#include "GenericTriggers.h"
#include "Playerbots.h"
#include "SharedDefines.h"
#include <string>

bool DKPresenceTrigger::IsActive()
{
    Unit* target = GetTarget();
    return !botAI->HasAura("blood presence", target) && !botAI->HasAura("unholy presence", target) &&
           !botAI->HasAura("frost presence", target);
}

bool PestilenceGlyphTrigger::IsActive()
{
    if (!SpellTrigger::IsActive())
    {
        return false;
    }
    if (!bot->HasAura(63334))
    {
        return false;
    }
    Aura* blood_plague = botAI->GetAura("blood plague", GetTarget(), true, true);
    Aura* frost_fever = botAI->GetAura("frost fever", GetTarget(), true, true);
    if ((blood_plague && blood_plague->GetDuration() <= 3000) || (frost_fever && frost_fever->GetDuration() <= 3000))
    {
        return true;
    }
    return false;
}

// Based on runeSlotTypes
bool HighBloodRuneTrigger::IsActive()
{
    return bot->GetRuneCooldown(0) <= 2000 && bot->GetRuneCooldown(1) <= 2000;
}

bool HighFrostRuneTrigger::IsActive()
{
    return bot->GetRuneCooldown(4) <= 2000 && bot->GetRuneCooldown(5) <= 2000;
}

bool HighUnholyRuneTrigger::IsActive()
{
    return bot->GetRuneCooldown(2) <= 2000 && bot->GetRuneCooldown(3) <= 2000;
}

bool NoRuneTrigger::IsActive()
{
    for (uint32 i = 0; i < MAX_RUNES; ++i)
    {
        if (!bot->GetRuneCooldown(i))
            return false;
    }
    return true;
}

bool DesolationTrigger::IsActive()
{
    return bot->HasAura(66817) && BuffTrigger::IsActive();
}

bool DeathAndDecayCooldownTrigger::IsActive()
{
    uint32 spellId = AI_VALUE2(uint32, "spell id", name);
    if (!spellId)
        return true;

    return bot->GetSpellCooldownDelay(spellId) >= 2000;
}

bool DKPlayerTargetOutOfMeleeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || !target->IsPlayer() || bot->IsFriendlyTo(target))
        return false;

    return !bot->IsWithinMeleeRange(target);
}

bool ChainsOfIceKiteTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsPlayer() || bot->IsFriendlyTo(target))
        return false;

    return DebuffTrigger::IsActive();
}

bool AntiMagicShellTrigger::IsActive()
{
    if (!BuffTrigger::IsActive())
        return false;

    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    for (ObjectGuid const guid : attackers)
    {
        Unit* attacker = botAI->GetUnit(guid);
        if (!attacker || !attacker->IsAlive())
            continue;

        if (attacker->IsNonMeleeSpellCast(true) && attacker->GetTarget() == bot->GetGUID())
            return true;
    }

    return false;
}

bool DKStunnedTrigger::IsActive() { return bot->IsInCombat() && bot->HasUnitState(UNIT_STATE_STUNNED); }

bool HungeringColdTrigger::IsActive()
{
    if (!botAI->HasPvpOpponent())
        return false;

    uint32 count = 0;
    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    for (ObjectGuid const guid : attackers)
    {
        Unit* attacker = botAI->GetUnit(guid);
        if (!attacker || !attacker->IsAlive() || !attacker->IsControlledByPlayer())
            continue;

        if (AttackersValue::IsCrowdControlled(attacker))
            continue;

        if (bot->GetDistance(attacker) <= 10.0f)
            ++count;
    }

    return count >= 2;
}
