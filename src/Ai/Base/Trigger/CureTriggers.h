/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CURETRIGGERS_H
#define PLAYERBOTS_CURETRIGGERS_H

#include "GenericTriggers.h"

class PlayerbotAI;
class Unit;

class NeedCureTrigger : public SpellTrigger
{
public:
    NeedCureTrigger(PlayerbotAI* botAI, std::string const spell, uint32 dispelType, int32 checkInterval = 1 * 1000)
        : SpellTrigger(botAI, spell, checkInterval), dispelType(dispelType)
    {
    }

    std::string const GetTargetName() override { return "self target"; }
    bool IsActive() override;
    ReactionCategory GetReactionCategory() override { return REACTION_DISPEL; }

protected:
    uint32 dispelType;
};

// Offensive dispel (Purge, Spellsteal, Devour Magic, Tranquilizing Shot): only fires on
// high-value buffs (see PlayerbotAI::HasAuraToDispel), checks every 5s rather than every
// second, and is skipped below 40% mana so it can't starve the damage kit.
class TargetAuraDispelTrigger : public NeedCureTrigger
{
public:
    TargetAuraDispelTrigger(PlayerbotAI* botAI, std::string const spell, uint32 dispelType)
        : NeedCureTrigger(botAI, spell, dispelType, 5 * 1000)
    {
    }

    std::string const GetTargetName() override { return "current target"; }
    bool IsActive() override;
};

class PartyMemberNeedCureTrigger : public NeedCureTrigger
{
public:
    PartyMemberNeedCureTrigger(PlayerbotAI* botAI, std::string const spell, uint32 dispelType)
        : NeedCureTrigger(botAI, spell, dispelType)
    {
    }

    Value<Unit*>* GetTargetValue() override;
    bool IsActive() override;
};

class NeedWorldBuffTrigger : public Trigger
{
public:
    NeedWorldBuffTrigger(PlayerbotAI* botAI) : Trigger(botAI) {}

    bool IsActive() override;
};

#endif
