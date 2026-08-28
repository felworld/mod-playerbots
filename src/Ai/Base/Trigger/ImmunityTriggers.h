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

class Aura;
class PlayerbotAI;

// An immunity the bot is wearing has outlived its use: it is now costing the bot something
// (Limiting) and dropping it would not get the bot killed ("safe to drop immunity"). Both halves
// are positive reasons - the trigger never fires merely because the emergency has passed, since a
// bubble that costs nothing and hurts nobody may as well run out (Felworld).
class OutlivedImmunityTrigger : public Trigger
{
public:
    OutlivedImmunityTrigger(PlayerbotAI* botAI, std::string const name, std::vector<uint32> spellIds)
        : Trigger(botAI, name), spellIds(std::move(spellIds)) {}

    bool IsActive() override;

protected:
    // Whether "cancel immunity" is responsible for this aura at all. Default: yes.
    virtual bool Managed(Aura const* /*aura*/) { return true; }
    // Whether the aura is currently holding the bot back.
    virtual bool Limiting() { return true; }
    virtual bool Safe();

    bool OutOfCombat();

private:
    std::vector<uint32> spellIds;
};

// An immunity the bot cast itself. Only managed when it came from one of the survival triggers
// (see ai::immunity::IsSurvivalTrigger) - the record left by CastEmergencyImmunityAction.
class OwnImmunityOutlivedTrigger : public OutlivedImmunityTrigger
{
public:
    using OutlivedImmunityTrigger::OutlivedImmunityTrigger;

protected:
    bool Managed(Aura const* aura) override;
};

class IceBlockOutlivedTrigger : public OwnImmunityOutlivedTrigger
{
public:
    IceBlockOutlivedTrigger(PlayerbotAI* botAI);
};

class DivineShieldOutlivedTrigger : public OwnImmunityOutlivedTrigger
{
public:
    DivineShieldOutlivedTrigger(PlayerbotAI* botAI);

protected:
    bool Limiting() override;
};

class DispersionOutlivedTrigger : public OwnImmunityOutlivedTrigger
{
public:
    DispersionOutlivedTrigger(PlayerbotAI* botAI);

protected:
    bool Limiting() override;
};

class DivineInterventionOutlivedTrigger : public OutlivedImmunityTrigger
{
public:
    DivineInterventionOutlivedTrigger(PlayerbotAI* botAI);

protected:
    bool Safe() override;
};

class HandOfProtectionOutlivedTrigger : public OutlivedImmunityTrigger
{
public:
    HandOfProtectionOutlivedTrigger(PlayerbotAI* botAI);

protected:
    bool Limiting() override;
};

// The other side of the same coin: an immunity the bot put on somebody else. The mob a warlock
// banished is immune to everything, so once the rest of the pull is dead the group is left standing
// over a mob it cannot touch until the aura runs out. Releasing it is the warlock's job (Felworld).
class BanishOutlivedTrigger : public Trigger
{
public:
    BanishOutlivedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "banish outlived") {}

    bool IsActive() override;
};

#endif
