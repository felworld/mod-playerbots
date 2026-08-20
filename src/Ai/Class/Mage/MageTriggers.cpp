/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MageTriggers.h"
#include "AttackersValue.h"
#include "DynamicObject.h"
#include "Player.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "Value.h"

namespace
{
// Frost Nova's radius.
constexpr float FROST_NOVA_RANGE = 10.0f;

// Polymorph on a player halves in duration on the second application inside the diminishing
// window and is ignored on the third. A sheep that lands for a second is not a setup.
bool IsPolymorphDiminished(Unit* target, uint32 polymorphSpellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(polymorphSpellId);
    if (!spellInfo)
        return true;

    DiminishingGroup group = GetDiminishingReturnsGroupForSpell(spellInfo, false);
    return group != DIMINISHING_NONE && target->GetDiminishing(group) >= DIMINISHING_LEVEL_2;
}
}

bool NoManaGemTrigger::IsActive()
{
    static const std::vector<uint32> gemIds = {
        33312,  // Mana Sapphire
        22044,  // Mana Emerald
        8008,   // Mana Ruby
        8007,   // Mana Citrine
        5513,   // Mana Jade
        5514    // Mana Agate
    };

    for (uint32 gemId : gemIds)
    {
        if (bot->GetItemCount(gemId, false) > 0)  // false = only in bags
            return false;
    }
    return true;
}

bool ArcaneIntellectTrigger::IsActive()
{
    return BuffTrigger::IsActive() && !botAI->HasAura("arcane brilliance", GetTarget());
}

bool MageArmorTrigger::IsActive()
{
    Unit* target = GetTarget();
    return botAI->HasSpell("mage armor") && !botAI->HasAura("mage armor", target) &&
           !botAI->HasAura("ice armor", target) && !botAI->HasAura("frost armor", target) &&
           !botAI->HasAura("molten armor", target);
}

bool MoltenArmorTrigger::IsActive()
{
    Unit* target = GetTarget();
    return botAI->HasSpell("molten armor") && !botAI->HasAura("molten armor", target) &&
           !botAI->HasAura("ice armor", target) && !botAI->HasAura("frost armor", target) &&
           !botAI->HasAura("mage armor", target);
}

bool FrostNovaOnTargetTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    return botAI->HasAura(spell, target);
}

bool FrostbiteOnTargetTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    return botAI->HasAura(spell, target);
}

bool NoFocusMagicTrigger::IsActive()
{
    constexpr uint32 SPELL_FOCUS_MAGIC = 54646;
    if (!bot->HasSpell(SPELL_FOCUS_MAGIC))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        if (member->HasAura(SPELL_FOCUS_MAGIC, bot->GetGUID()))
            return false;
    }
    return true;
}

bool DeepFreezeCooldownTrigger::IsActive()
{
    constexpr uint32 SPELL_DEEP_FREEZE = 44572;
    return !bot->HasSpell(SPELL_DEEP_FREEZE) ||
           SpellCooldownTrigger::IsActive();
}

const std::unordered_set<uint32> FlamestrikeNearbyTrigger::FLAMESTRIKE_SPELL_IDS = {
    2120, 2121, 8422, 8423, 10215, 10216, 27086, 42925, 42926
};

bool FlamestrikeNearbyTrigger::IsActive()
{
    for (uint32 spellId : FLAMESTRIKE_SPELL_IDS)
    {
        Aura* aura = bot->GetAura(spellId, bot->GetGUID());
        if (!aura)
            continue;

        DynamicObject* dynObj = aura->GetDynobjOwner();
        if (!dynObj)
            continue;

        float dist = bot->GetDistance2d(dynObj->GetPositionX(), dynObj->GetPositionY());
        if (dist <= radius)
            return true;
    }
    return false;
}

bool ImprovedScorchTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    static const uint32 ImprovedScorchExclusiveDebuffs[] = {// Shadow Mastery
                                                            17794, 17797, 17798, 17799, 17800,
                                                            // Winter's Chill
                                                            12579,
                                                            // Improved Scorch
                                                            22959};

    for (uint32 spellId : ImprovedScorchExclusiveDebuffs)
    {
        if (target->HasAura(spellId))
            return false;
    }

    return DebuffTrigger::IsActive();
}

const std::unordered_set<uint32> BlizzardChannelCheckTrigger::BLIZZARD_SPELL_IDS = {
    10,     // Blizzard Rank 1
    6141,   // Blizzard Rank 2
    8427,   // Blizzard Rank 3
    10185,  // Blizzard Rank 4
    10186,  // Blizzard Rank 5
    10187,  // Blizzard Rank 6
    27085,  // Blizzard Rank 7
    42938,  // Blizzard Rank 8
    42939   // Blizzard Rank 9
};

bool BlizzardChannelCheckTrigger::IsActive()
{
    if (Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        spell && BLIZZARD_SPELL_IDS.count(spell->m_spellInfo->Id))
    {
        uint8 attackerCount = AI_VALUE(uint8, "attacker count");
        return attackerCount < minEnemies;
    }

    return false;
}

bool MeleeAttackerInNovaRangeTrigger::IsActive()
{
    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    for (ObjectGuid const guid : attackers)
    {
        Unit* attacker = botAI->GetUnit(guid);
        if (!attacker || !attacker->IsAlive() || attacker->isFrozen())
            continue;

        // Rooting something the group is holding elsewhere buys nothing and costs threat.
        if (attacker->GetVictim() != bot)
            continue;

        if (bot->IsWithinCombatRange(attacker, FROST_NOVA_RANGE))
            return true;
    }

    return false;
}

bool PolymorphOpenerTrigger::IsActive()
{
    // A setup, not a panic button: an even 1v1 the bot is starting from full strength. A hurt
    // bot is served by the shared "cc target" breather instead.
    if (bot->GetGroup() || static_cast<uint8>(bot->GetHealthPct()) < sPlayerbotAIConfig.almostFullHealth)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || !target->IsControlledByPlayer())
        return false;

    if (static_cast<uint8>(target->GetHealthPct()) < sPlayerbotAIConfig.almostFullHealth)
        return false;

    if (AI_VALUE(GuidVector, "attackers").size() > 1)
        return false;

    // No point sheeping without the payoff, and none at all if the sheep would not stick.
    if (!botAI->CanCastSpell("pyroblast", target) || !botAI->CanCastSpell("polymorph", target))
        return false;

    if (AttackersValue::IsCrowdControlled(target))
        return false;

    return !IsPolymorphDiminished(target, AI_VALUE2(uint32, "spell id", "polymorph"));
}

bool PolymorphedOpponentTrigger::IsActive()
{
    Unit* sheep = AI_VALUE2(Unit*, "current cc target", "polymorph");
    if (!sheep || !sheep->IsAlive() || !sheep->IsControlledByPlayer())
        return false;

    if (bot->GetGroup() || !AI_VALUE(GuidVector, "attackers").empty())
        return false;

    // Never undo the shared breather sheep, which is cast exactly when the bot is hurt or out of
    // mana and exists to end the fight rather than open one.
    if (static_cast<uint8>(bot->GetHealthPct()) < sPlayerbotAIConfig.mediumHealth ||
        AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.lowMana)
        return false;

    return botAI->CanCastSpell("pyroblast", sheep);
}

bool SlowKiteTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsControlledByPlayer() || bot->IsFriendlyTo(target))
        return false;

    return DebuffTrigger::IsActive();
}
