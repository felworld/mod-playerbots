/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GenericActions.h"
#include "CharmInfo.h"
#include "CreatureAI.h"
#include "DungeonHoldValues.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "PullStrategy.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <vector>

enum PetSpells
{
    PET_PROWL_1 = 24450,
    PET_PROWL_2 = 24452,
    PET_PROWL_3 = 24453,
    PET_COWER = 1742,
    PET_LEAP = 47482,
    PET_SPELL_LOCK_1 = 19244,
    PET_SPELL_LOCK_2 = 19647,
    PET_DEVOUR_MAGIC_1 = 19505,
    PET_DEVOUR_MAGIC_2 = 19731,
    PET_DEVOUR_MAGIC_3 = 19734,
    PET_DEVOUR_MAGIC_4 = 19736,
    PET_DEVOUR_MAGIC_5 = 27276,
    PET_DEVOUR_MAGIC_6 = 27277,
    PET_DEVOUR_MAGIC_7 = 48011,
    PET_SPIRIT_WOLF_LEAP = 58867
};

static std::vector<uint32> disabledPetSpells = {
    PET_PROWL_1, PET_PROWL_2, PET_PROWL_3,
    PET_COWER, PET_LEAP,
    PET_SPELL_LOCK_1, PET_SPELL_LOCK_2,
    PET_DEVOUR_MAGIC_1, PET_DEVOUR_MAGIC_2, PET_DEVOUR_MAGIC_3,
    PET_DEVOUR_MAGIC_4, PET_DEVOUR_MAGIC_5, PET_DEVOUR_MAGIC_6, PET_DEVOUR_MAGIC_7, PET_SPIRIT_WOLF_LEAP
};

namespace
{
// Taunt detection follows the core (Spell::EffectTaunt uses SPELL_EFFECT_ATTACK_ME, PetAI keys taunt
// behaviour off SPELL_AURA_MOD_TAUNT). SPELL_EFFECT_THREAT is added because on pets it only ever
// backs a threat-grabbing ability (Growl, Torment, Suffering, Anguish), which is what we want off.
bool IsPetTauntSpell(SpellInfo const* spellInfo)
{
    return spellInfo->HasEffect(SPELL_EFFECT_ATTACK_ME) || spellInfo->HasAura(SPELL_AURA_MOD_TAUNT) ||
           spellInfo->HasEffect(SPELL_EFFECT_THREAT);
}

// A player opponent seen this recently still counts as one, so a fight that alternates between
// players and their pets/mobs does not flap the pet bar every few seconds.
constexpr uint32 PVP_TAUNT_SUPPRESSION_LINGER_MS = 10 * IN_MILLISECONDS;

// PullStrategy parks the pet on REACT_PASSIVE for the duration of a pull; stance upkeep must not
// undo that mid-pull.
bool IsPullInProgress(PlayerbotAI* botAI)
{
    PullStrategy* strategy = PullStrategy::Get(botAI);
    return strategy && (strategy->IsPullPendingToStart() || strategy->HasPullStarted() || strategy->HasTarget());
}
}

bool MeleeAction::isUseful()
{
    // do not allow if can't attack from vehicle
    if (botAI->IsInVehicle() && !botAI->IsInVehicle(false, false, true))
        return false;

    // Do not start autoattack while prowled — let opener spells break stealth intentionally.
    // Future rogue stealth implementation should use this instead:
    // return !(botAI->HasAura("stealth", bot) || botAI->HasAura("prowl", bot));
    return !botAI->HasAura("prowl", bot);
}

bool TogglePetSpellAutoCastAction::SuppressTaunts(Pet* pet)
{
    // Inside an instance with a group the tank owns threat: actively turn taunt autocasts off so a
    // previously enabled Growl/Torment/Suffering/Anguish stops ripping mobs off the tank. Solo (or
    // outdoors) they are left enabled - a lone pet tanking for its owner still wants them.
    if (IsInstancedGroupContent(bot))
        return true;

    // Players have no threat list, so a taunt lands as a pure no-op that still eats the pet's global
    // cooldown and delays Bite/Claw/Kill Command. A battleground or arena is PvP from the start; out
    // in the world it is whether a player (or a player's pet) is on the other side of the fight.
    if (bot->InBattleground() || bot->InArena())
        return true;

    // HasPvpOpponent() reads the bot's own target and attackers; the pet can be swinging at someone
    // else entirely (a warlock minion left on a previous target), and it is the pet's global cooldown
    // being spent, so its own victim gets the same owner check.
    Unit* petVictim = pet->GetVictim();
    Player* petVictimOwner = petVictim ? petVictim->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;

    uint32 const now = getMSTime();
    if (botAI->HasPvpOpponent() || (petVictimOwner && !bot->IsFriendlyTo(petVictimOwner)))
    {
        lastPvpOpponentMs = now;
        return true;
    }

    return lastPvpOpponentMs && getMSTimeDiff(lastPvpOpponentMs, now) < PVP_TAUNT_SUPPRESSION_LINGER_MS;
}

bool TogglePetSpellAutoCastAction::Execute(Event /*event*/)
{
    Pet* pet = bot->GetPet();
    if (!pet)
    {
        return false;
    }
    // hack on high level spell after low level initialization
    std::vector<unsigned int> shouldRemove;
    for (unsigned int& m_autospell : pet->m_autospells)
    {
        if (!pet->HasSpell(m_autospell))
        {
            shouldRemove.push_back(m_autospell);
        }
    }
    for (unsigned int spellId : shouldRemove)
    {
        auto autospellItr = std::find(pet->m_autospells.begin(), pet->m_autospells.end(), spellId);
        if (autospellItr != pet->m_autospells.end())
            pet->m_autospells.erase(autospellItr);
    }
    bool const suppressTaunts = SuppressTaunts(pet);

    bool toggled = false;
    for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
    {
        if (itr->second.state == PETSPELL_REMOVED)
            continue;

        uint32 spellId = itr->first;
        const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || !spellInfo->IsAutocastable())
            continue;

        bool shouldApply = true;
        for (uint32 disabledSpell : disabledPetSpells)
        {
            if (spellId == disabledSpell)
            {
                shouldApply = false;
                break;
            }
        }
        if (shouldApply && suppressTaunts && IsPetTauntSpell(spellInfo))
            shouldApply = false;

        bool isAutoCast = false;
        for (unsigned int& m_autospell : pet->m_autospells)
        {
            if (m_autospell == spellId)
            {
                isAutoCast = true;
                break;
            }
        }
        if (shouldApply != isAutoCast)
        {
            pet->ToggleAutocast(spellInfo, shouldApply);
            toggled = true;
        }
    }

    // Debug message if pet spells have been toggled and debug is enabled
    if (toggled && sPlayerbotAIConfig.petChatCommandDebug == 1)
        botAI->TellMaster("Pet autocast spells have been toggled.");

    return toggled;
}

bool PetAttackAction::isUseful()
{
    Guardian* pet = bot->GetGuardianPet();
    if (!pet || !pet->IsAlive())
        return false;

    // Uncontrollable guardians (temporary summons without a pet bar) have no CharmInfo.
    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
        return false;

    // A passive pet is either configured that way or parked by PullStrategy - do not fight it.
    if (pet->GetReactState() == REACT_PASSIVE)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    // Already assisting on the right target - re-issuing the command would only cost a tick.
    if (pet->GetVictim() == target && charmInfo->IsCommandAttack())
        return false;

    if (!bot->IsValidAttackTarget(target))
        return false;

    // Assist only: in an instance with a group the pet must never be the one starting a fight, and
    // it stays on its leash until the main tank has the mob (Felworld).
    if (IsInstancedGroupContent(bot) && (!target->IsInCombat() || ShouldHoldForTank(botAI, target)))
        return false;

    return true;
}

bool PetAttackAction::Execute(Event /*event*/)
{
    Guardian* pet = bot->GetGuardianPet();
    if (!pet)
        return false;

    // Do not attack if the pet's stance is set to "passive".
    if (pet->GetReactState() == REACT_PASSIVE)
        return false;

    // Uncontrollable guardians (temporary summons without a pet bar) have no CharmInfo - nothing to
    // command, and dereferencing it would crash.
    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    if (!bot->IsValidAttackTarget(target))
        return false;

    // Assist only: in an instance with a group the pet must never be the one starting a fight, and
    // it stays on its leash until the main tank has the mob (Felworld).
    if (IsInstancedGroupContent(bot) && (!target->IsInCombat() || ShouldHoldForTank(botAI, target)))
        return false;

    // This section has been commented because it was overriding the
    // pet's stance to "passive" every time the attack action was executed.
    // pet->SetReactState(REACT_PASSIVE);

    pet->ClearUnitState(UNIT_STATE_FOLLOW);
    pet->AttackStop();
    pet->SetTarget(target->GetGUID());

    charmInfo->SetIsCommandAttack(true);
    charmInfo->SetIsAtStay(false);
    charmInfo->SetIsFollowing(false);
    charmInfo->SetIsCommandFollow(false);
    charmInfo->SetIsReturning(false);

    pet->ToCreature()->AI()->AttackStart(target);
    return true;
}

bool SetPetStanceAction::Execute(Event /*event*/)
{
    // A pull deliberately parks the pet on REACT_PASSIVE (PullStartAction) and restores the previous
    // stance when it ends (PullEndAction) - stay out of the way until then.
    if (IsPullInProgress(botAI))
        return false;

    // Prepare a list to hold all controlled pet and guardian creatures
    std::vector<Creature*> targets;

    // Add the bot's main pet (if it exists) to the target list
    Pet* pet = bot->GetPet();
    if (pet)
        targets.push_back(pet);

    // Loop through all units controlled by the bot (could be pets, guardians, etc.)
    for (Unit::ControlSet::const_iterator itr = bot->m_Controlled.begin(); itr != bot->m_Controlled.end(); ++itr)
    {
        // Only add creatures (skip players, vehicles, etc.)
        Creature* creature = dynamic_cast<Creature*>(*itr);
        if (!creature)
            continue;
        // Avoid adding the main pet twice
        if (pet && creature == pet)
            continue;
        targets.push_back(creature);
    }

    // If there are no controlled pets or guardians, notify the player and exit
    if (targets.empty())
    {
        botAI->TellError(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "pet_no_pet_error", "You have no pet or guardian pet.", {}));
        return false;
    }

    // Get the default pet stance from the configuration
    int32 stance = sPlayerbotAIConfig.defaultPetStance;
    ReactStates react = REACT_DEFENSIVE;
    std::string stanceText = "defensive (from config, fallback)";

    // Map the config stance integer to a ReactStates value and a message
    switch (stance)
    {
        case 0:
            react = REACT_PASSIVE;
            stanceText = "passive (from config)";
            break;
        case 1:
            react = REACT_DEFENSIVE;
            stanceText = "defensive (from config)";
            break;
        case 2:
            react = REACT_AGGRESSIVE;
            stanceText = "aggressive (from config)";
            break;
        default:
            react = REACT_DEFENSIVE;
            stanceText = "defensive (from config, fallback)";
            break;
    }

    // An aggressive pet in a group pulls whatever wanders into its range - clamp it to defensive so
    // it only answers what is already attacking the bot (or what the bot is told to assist).
    if (react == REACT_AGGRESSIVE && bot->GetGroup())
    {
        react = REACT_DEFENSIVE;
        stanceText = "defensive (aggressive clamped while grouped)";
    }

    // Apply the stance to all target creatures (pets/guardians). This runs on a repeating trigger, so
    // only touch creatures that are actually on the wrong stance.
    bool changed = false;
    for (Creature* target : targets)
    {
        if (target->GetReactState() == react)
            continue;

        target->SetReactState(react);
        CharmInfo* charmInfo = target->GetCharmInfo();
        // If the creature has a CharmInfo, set the player-visible stance as well
        if (charmInfo)
            charmInfo->SetPlayerReactState(react);

        changed = true;
    }

    if (!changed)
        return false;

    // If debug is enabled in config, inform the master of the new stance
    if (sPlayerbotAIConfig.petChatCommandDebug == 1)
        botAI->TellMaster("Pet stance set to " + stanceText + " (applied to all pets/guardians).");

    return true;
}
