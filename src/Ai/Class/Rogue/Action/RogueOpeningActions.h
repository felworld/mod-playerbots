/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ROGUEOPENINGACTIONS_H
#define PLAYERBOTS_ROGUEOPENINGACTIONS_H

#include "GenericSpellActions.h"

class PlayerbotAI;
class Unit;

class CastSapAction : public CastMeleeSpellAction
{
public:
    CastSapAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "sap") {}

    Value<Unit*>* GetTargetValue() override;
    bool isUseful() override { return true; }
};

// Sap the hostile player standing next to the mark rather than the mark itself: the shared "cc
// target" value only offers units already attacking the bot, which Sap can never take. Casting
// Sap does not break stealth, so the bot walks on and opens on the mark one-on-one.
class CastSapOpenerAction : public CastSpellAction
{
public:
    CastSapOpenerAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "sap") {}

    static Unit* FindSapTarget(PlayerbotAI* botAI);

    std::string const getName() override { return "sap opener"; }
    Unit* GetTarget() override { return FindSapTarget(botAI); }
};

class CastGarroteAction : public CastDebuffSpellAction
{
public:
    CastGarroteAction(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "garrote", true, 8.0f) {}
};

class CastCheapShotAction : public CastMeleeSpellAction
{
public:
    CastCheapShotAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "cheap shot") {}
};

class CastAmbushAction : public CastMeleeSpellAction
{
public:
    CastAmbushAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "ambush") {}
};

#endif
