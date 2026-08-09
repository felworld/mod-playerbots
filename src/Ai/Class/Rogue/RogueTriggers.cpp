/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RogueTriggers.h"

#include "Formulas.h"
#include "GenericTriggers.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "SpellMgr.h"

namespace
{
constexpr uint32 SPELL_STEALTH = 1784;
constexpr uint32 SPELL_SPRINT_RANK_1 = 2983;

// The Distract cast window: close enough that the dest point (5yd past
// the target) stays inside the spell's 30yd range, far enough that the
// turn still buys a real approach.
constexpr float DISTRACT_MIN_RANGE = 8.0f;
constexpr float DISTRACT_MAX_RANGE = 24.0f;

// First ranks of the persistent ground effects whose presence makes
// Distract pointless - a defender sweeping the approach with area damage
// or detection doesn't care which way they're facing. All of it is
// on-screen information: the glowing patch, the flare, the planted totem,
// and the armed trap (visible to a rogue through Detect Traps).
constexpr uint32 DISTRACT_DYNOBJ_VETOES[] = {
    26573,  // Consecration
    43265,  // Death and Decay
    1543,   // Flare
};
constexpr uint32 DISTRACT_TRAP_VETOES[] = {
    13795,  // Immolation Trap
    13813,  // Explosive Trap
};

bool HasFlushAoeDeployed(Unit* target)
{
    for (uint32 first : DISTRACT_DYNOBJ_VETOES)
        for (uint32 id = first; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (target->GetDynObject(id))
                return true;

    for (uint32 first : DISTRACT_TRAP_VETOES)
        for (uint32 id = first; id; id = sSpellMgr->GetNextSpellInChain(id))
            if (target->GetGameObject(id))
                return true;

    // Any fire totem: Magma pulses on its own, and 3.3.5 Fire Nova
    // detonates off whichever fire totem is planted.
    return !target->m_SummonSlot[SUMMON_SLOT_TOTEM_FIRE].IsEmpty();
}
}

// bool AdrenalineRushTrigger::isPossible()
// {
//     return !botAI->HasAura("stealth", bot);
// }

bool UnstealthTrigger::IsActive()
{
    if (!botAI->HasAura("stealth", bot))
        return false;

    return botAI->HasAura("stealth", bot) && !AI_VALUE(uint8, "attacker count") &&
           (AI_VALUE2(bool, "moving", "self target") &&
            ((botAI->GetMaster() &&
              ServerFacade::instance().IsDistanceGreaterThan(AI_VALUE2(float, "distance", "group leader"), 10.0f) &&
              AI_VALUE2(bool, "moving", "group leader")) ||
             !AI_VALUE(uint8, "attacker count")));
}

bool StealthTrigger::IsActive()
{
    if (bot->HasAura(SPELL_STEALTH) || bot->IsInCombat() || bot->HasSpellCooldown(SPELL_STEALTH))
        return false;

    // Sneak into the enemy flag room even when no enemy has been spotted yet
    if (AI_VALUE(bool, "near enemy flag room"))
        return true;

    float distance = 30.f;

    Unit* target = AI_VALUE(Unit*, "enemy player target");
    if (target && !target->IsInWorld())
    {
        return false;
    }
    if (!target)
        target = AI_VALUE(Unit*, "grind target");

    if (!target)
        target = AI_VALUE(Unit*, "dps target");

    if (!target)
        return false;

    if (target && target->GetVictim())
        distance -= 10;

    if (target->isMoving() && target->GetVictim())
        distance -= 10;

    if (bot->InBattleground())
        distance += 15;

    if (bot->InArena())
        distance += 15;

    return target && ServerFacade::instance().GetDistance2d(bot, target) < distance;
}

bool SapTrigger::IsPossible() { return bot->GetLevel() > 10 && botAI->HasSpell("sap") && !bot->IsInCombat(); }

bool SprintTrigger::IsPossible() { return bot->HasSpell(SPELL_SPRINT_RANK_1); }

bool SprintTrigger::IsActive()
{
    if (bot->HasSpellCooldown(SPELL_SPRINT_RANK_1))
        return false;

    float distance = botAI->GetMaster() ? 45.0f : 35.0f;
    if (botAI->HasAura("stealth", bot))
        distance -= 10;

    bool targeted = false;

    Unit* dps = AI_VALUE(Unit*, "dps target");
    Unit* enemyPlayer = AI_VALUE(Unit*, "enemy player target");

    if (enemyPlayer && !enemyPlayer->IsInWorld())
    {
        return false;
    }
    if (dps)
        targeted = (dps == AI_VALUE(Unit*, "current target"));

    if (enemyPlayer && !targeted)
        targeted = (enemyPlayer == AI_VALUE(Unit*, "current target"));

    if (!targeted)
        return false;

    if ((dps && dps->IsInCombat()) || enemyPlayer)
        distance -= 10;

    return AI_VALUE2(bool, "moving", "self target") &&
           (AI_VALUE2(bool, "moving", "dps target") || AI_VALUE2(bool, "moving", "enemy player target")) && targeted &&
           (ServerFacade::instance().IsDistanceGreaterThan(AI_VALUE2(float, "distance", "dps target"), distance) ||
            ServerFacade::instance().IsDistanceGreaterThan(AI_VALUE2(float, "distance", "enemy player target"), distance));
}

bool ExposeArmorTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return DebuffTrigger::IsActive() && !botAI->HasAura("sunder armor", target, false, false, -1, true) &&
           AI_VALUE2(uint8, "combo", "current target") <= 3;
}

bool DistractTrigger::IsActive()
{
    // Whether this rogue knows the trick at all: a stable per-character
    // roll, so the same rogue is consistently tricky or consistently not.
    if (bot->GetGUID().GetCounter() % 100 >= sPlayerbotAIConfig.rogueDistractChance)
        return false;

    if (!bot->HasStealthAura())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsPlayer() || !target->IsAlive() || target->GetMap() != bot->GetMap())
        return false;

    // Only when sneaking is contested (the bot is crossing the target's
    // front arc) and the target is dangerous enough to respect: yellow
    // con or above.
    if (!target->HasInArc(M_PI, bot))
        return false;

    XPColorChar const color = Acore::XP::GetColorCode(bot->GetLevel(), target->GetLevel());
    if (color == XP_GREEN || color == XP_GRAY)
        return false;

    // Mirror EffectDistract's own gates so the cast is never a no-op, and
    // skip targets already turned or bearing down on someone.
    if (target->IsEngaged() || target->isMoving() ||
        target->HasUnitState(UNIT_STATE_DISTRACTED | UNIT_STATE_CONFUSED | UNIT_STATE_STUNNED | UNIT_STATE_FLEEING))
        return false;

    // The trick is paid out of overcap only: the opener and everything
    // after it want the full pool, so below cap just sneak.
    if (bot->GetPower(POWER_ENERGY) < bot->GetMaxPower(POWER_ENERGY))
        return false;

    if (HasFlushAoeDeployed(target))
        return false;

    float const dist = bot->GetExactDist(target);
    if (dist < DISTRACT_MIN_RANGE || dist > DISTRACT_MAX_RANGE)
        return false;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", "distract");
    return spellId && !bot->HasSpellCooldown(spellId);
}

bool MainHandWeaponNoEnchantTrigger::IsActive()
{
    Item* const itemForSpell = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!itemForSpell || itemForSpell->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        return false;
    return true;
}

bool OffHandWeaponNoEnchantTrigger::IsActive()
{
    Item* const itemForSpell = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!itemForSpell || itemForSpell->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        return false;
    return true;
}
