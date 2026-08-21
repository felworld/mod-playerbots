/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_IMMUNITYTRIGGERS_H
#define PLAYERBOTS_IMMUNITYTRIGGERS_H

#include "ImmunitySpells.h"
#include "Trigger.h"

#include <vector>

class PlayerbotAI;

// An immunity the bot is wearing has stopped paying for itself: the fight that called for it is
// over, or the bot has been healed back up and the aura now only keeps it from acting (Ice Block,
// Dispersion, Divine Intervention), halves its damage and sheds every mob's interest (Divine
// Shield), or stops a physical attacker from swinging (Hand of Protection) (Felworld).
class OutlivedImmunityTrigger : public Trigger
{
public:
    OutlivedImmunityTrigger(PlayerbotAI* botAI, std::string const name, std::vector<uint32> spellIds)
        : Trigger(botAI, name), spellIds(std::move(spellIds)) {}

    bool IsActive() override;

protected:
    virtual bool Outlived() = 0;

    bool OutOfCombat();
    // The aura came from the bot's own critical-health cast, not a scripted mechanic dodge.
    bool SurvivalCast();
    uint8 Health();
    uint8 Mana();

private:
    std::vector<uint32> spellIds;
};

class IceBlockOutlivedTrigger : public OutlivedImmunityTrigger
{
public:
    IceBlockOutlivedTrigger(PlayerbotAI* botAI);

protected:
    bool Outlived() override;
};

class DivineShieldOutlivedTrigger : public OutlivedImmunityTrigger
{
public:
    DivineShieldOutlivedTrigger(PlayerbotAI* botAI);

protected:
    bool Outlived() override;
};

class DispersionOutlivedTrigger : public OutlivedImmunityTrigger
{
public:
    DispersionOutlivedTrigger(PlayerbotAI* botAI);

protected:
    bool Outlived() override;
};

class DivineInterventionOutlivedTrigger : public OutlivedImmunityTrigger
{
public:
    DivineInterventionOutlivedTrigger(PlayerbotAI* botAI);

protected:
    bool Outlived() override;
};

class HandOfProtectionOutlivedTrigger : public OutlivedImmunityTrigger
{
public:
    HandOfProtectionOutlivedTrigger(PlayerbotAI* botAI);

protected:
    bool Outlived() override;
};

#endif
