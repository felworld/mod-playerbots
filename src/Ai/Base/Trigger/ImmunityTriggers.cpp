/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ImmunityTriggers.h"

#include "ImmunityValues.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SpellAuras.h"

bool OutlivedImmunityTrigger::IsActive()
{
    for (uint32 spellId : spellIds)
        if (Aura const* aura = bot->GetAura(spellId))
            return Managed(aura) && Limiting() && Safe();

    return false;
}

bool OutlivedImmunityTrigger::Safe() { return AI_VALUE(bool, "safe to drop immunity"); }

bool OutlivedImmunityTrigger::OutOfCombat() { return !AI_VALUE2(bool, "combat", "self target"); }

bool OwnImmunityOutlivedTrigger::Managed(Aura const* aura)
{
    ImmunityCast const& cast = AI_VALUE(ImmunityCast, "immunity cast");
    return ai::immunity::IsSurvivalTrigger(cast.trigger) && cast.Matches(aura);
}

IceBlockOutlivedTrigger::IceBlockOutlivedTrigger(PlayerbotAI* botAI)
    : OwnImmunityOutlivedTrigger(botAI, "ice block outlived", { ai::immunity::SPELL_ICE_BLOCK }) {}

// Ice Block: the mage can do nothing at all under it, so it is always limiting in a fight.

DivineShieldOutlivedTrigger::DivineShieldOutlivedTrigger(PlayerbotAI* botAI)
    : OwnImmunityOutlivedTrigger(botAI, "divine shield outlived", { ai::immunity::SPELL_DIVINE_SHIELD }) {}

// The bubble halves the paladin's damage and makes every mob walk past it (the core suppresses
// damage-immune victims on the threat list). That holds back a tank or a damage dealer; a healer
// heals just as well underneath and keeps it.
bool DivineShieldOutlivedTrigger::Limiting() { return !botAI->IsHeal(bot); }

DispersionOutlivedTrigger::DispersionOutlivedTrigger(PlayerbotAI* botAI)
    : OwnImmunityOutlivedTrigger(botAI, "dispersion outlived", { ai::immunity::SPELL_DISPERSION }) {}

// Dispersion locks the priest out of casting but refills its mana; it only holds the priest back
// once there is mana to cast with again.
bool DispersionOutlivedTrigger::Limiting()
{
    return AI_VALUE2(uint8, "mana", "self target") >= sPlayerbotAIConfig.mediumMana;
}

DivineInterventionOutlivedTrigger::DivineInterventionOutlivedTrigger(PlayerbotAI* botAI)
    : OutlivedImmunityTrigger(botAI, "divine intervention outlived",
                              { ai::immunity::SPELL_DIVINE_INTERVENTION }) {}

// Cast on the bot by a paladin who died to save it from a wipe. Dropping it mid-fight throws that
// away; it exists to be cancelled once the mobs have reset, so the survivor can resurrect the
// group - three minutes of waiting otherwise.
bool DivineInterventionOutlivedTrigger::Safe() { return OutOfCombat(); }

HandOfProtectionOutlivedTrigger::HandOfProtectionOutlivedTrigger(PlayerbotAI* botAI)
    : OutlivedImmunityTrigger(botAI, "hand of protection outlived", ai::immunity::HandOfProtectionRanks()) {}

// Physical immunity that also stops the wearer from melee or ranged attacks, and makes mobs drop
// it as a target. Holds back a tank or a physical damage dealer; casters lose nothing and keep it.
bool HandOfProtectionOutlivedTrigger::Limiting()
{
    return botAI->IsTank(bot) || botAI->IsMelee(bot) || bot->getClass() == CLASS_HUNTER;
}

// The banish is only in the way once nothing else is: while the rest of the pull is still swinging
// it is doing exactly the job it was cast for. Out of combat it is left alone too - it drops on its
// own there, and releasing it would hand the group back a mob it has walked away from.
bool BanishOutlivedTrigger::IsActive()
{
    if (!bot->IsInCombat() || !AI_VALUE(GuidVector, "attackers").empty())
        return false;

    BanishedTarget const banished = FindBanishedTarget(botAI);
    if (!banished)
        return false;

    // Nothing walks the group onto a mob nobody can attack, so the release waits until the bot can
    // reach and see it - following the master over is what closes that gap.
    return bot->IsWithinDistInMap(banished.unit, botAI->GetRange("spell")) &&
           bot->IsWithinLOSInMap(banished.unit) && !bot->HasSpellCooldown(banished.spellId);
}
