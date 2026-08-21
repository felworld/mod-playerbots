/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DKACTIONS_H
#define PLAYERBOTS_DKACTIONS_H

#include "Event.h"
#include "GenericSpellActions.h"

class PlayerbotAI;

class CastBloodPresenceAction : public CastBuffSpellAction
{
public:
    CastBloodPresenceAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "blood presence") {}
};

class CastFrostPresenceAction : public CastBuffSpellAction
{
public:
    CastFrostPresenceAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "frost presence") {}
};

class CastUnholyPresenceAction : public CastBuffSpellAction
{
public:
    CastUnholyPresenceAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "unholy presence") {}
};

// The dps presence choice. Blood Presence (damage and self-healing) is right in PvE, but against
// a player opponent Unholy Presence's run speed decides whether the bot is in melee at all, and
// a death knight out of melee does almost nothing.
class CastDpsPresenceAction : public CastBuffSpellAction
{
public:
    CastDpsPresenceAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "blood presence") {}

    std::string const getName() override { return "dps presence"; }
    bool isUseful() override;
    bool isPossible() override;
    bool Execute(Event event) override;

private:
    std::string const WantedPresence();
};

class CastDeathchillAction : public CastBuffSpellAction
{
public:
    CastDeathchillAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "deathchill") {}

    std::vector<NextAction> getPrerequisites() override;
};

class CastDarkCommandAction : public CastSpellAction
{
public:
    CastDarkCommandAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "dark command") {}
};

BEGIN_RANGED_SPELL_ACTION(CastDeathGripAction, "death grip")
END_SPELL_ACTION()

// Unholy presence
class CastUnholyMeleeSpellAction : public CastMeleeSpellAction
{
public:
    CastUnholyMeleeSpellAction(PlayerbotAI* botAI, std::string const spell) : CastMeleeSpellAction(botAI, spell) {}

    std::vector<NextAction> getPrerequisites() override;
};

// Frost presence
class CastFrostMeleeSpellAction : public CastMeleeSpellAction
{
public:
    CastFrostMeleeSpellAction(PlayerbotAI* botAI, std::string const spell) : CastMeleeSpellAction(botAI, spell) {}

    std::vector<NextAction> getPrerequisites() override;
};

// Blood presence
class CastBloodMeleeSpellAction : public CastMeleeSpellAction
{
public:
    CastBloodMeleeSpellAction(PlayerbotAI* botAI, std::string const spell) : CastMeleeSpellAction(botAI, spell) {}

    std::vector<NextAction> getPrerequisites() override;
};

class CastRuneStrikeAction : public CastMeleeSpellAction
{
public:
    CastRuneStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "rune strike") {}
};

class CastPestilenceAction : public CastSpellAction
{
public:
    CastPestilenceAction(PlayerbotAI* ai) : CastSpellAction(ai, "pestilence") {}
    ActionThreatType getThreatType() override { return ActionThreatType::None; }
};

class CastHowlingBlastAction : public CastSpellAction
{
public:
    CastHowlingBlastAction(PlayerbotAI* ai) : CastSpellAction(ai, "howling blast") {}
};

class CastIcyTouchAction : public CastSpellAction
{
public:
    CastIcyTouchAction(PlayerbotAI* ai) : CastSpellAction(ai, "icy touch") {}
};

class CastIcyTouchOnAttackerAction : public CastDebuffSpellOnAttackerAction
{
public:
    CastIcyTouchOnAttackerAction(PlayerbotAI* botAI)
        : CastDebuffSpellOnAttackerAction(botAI, "icy touch", true, .0f)
    {
    }
};

// debuff ps

class CastPlagueStrikeAction : public CastSpellAction
{
public:
    CastPlagueStrikeAction(PlayerbotAI* ai) : CastSpellAction(ai, "plague strike") {}
};

class CastPlagueStrikeOnAttackerAction : public CastDebuffSpellOnMeleeAttackerAction
{
public:
    CastPlagueStrikeOnAttackerAction(PlayerbotAI* botAI)
        : CastDebuffSpellOnMeleeAttackerAction(botAI, "plague strike", true, .0f)
    {
    }
};

// debuff
BEGIN_DEBUFF_ACTION(CastMarkOfBloodAction, "mark of blood")
END_SPELL_ACTION()

class CastMarkOfBloodOnAttackerAction : public CastDebuffSpellOnAttackerAction
{
public:
    CastMarkOfBloodOnAttackerAction(PlayerbotAI* botAI) : CastDebuffSpellOnAttackerAction(botAI, "mark of blood", true)
    {
    }
};

class CastSummonGargoyleAction : public CastSpellAction
{
public:
    CastSummonGargoyleAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "summon gargoyle") {}
};

class CastGhoulFrenzyAction : public CastBuffSpellAction
{
public:
    CastGhoulFrenzyAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "ghoul frenzy", false, 5000) {}
    std::string const GetTargetName() override { return "pet target"; }
};

BEGIN_MELEE_SPELL_ACTION(CastCorpseExplosionAction, "corpse explosion")
END_SPELL_ACTION()

// Anti-Magic Shell is cast on the death knight: as a melee action it could only fire when the
// caster we want to shrug off happened to be standing next to us.
class CastAntiMagicShellAction : public CastBuffSpellAction
{
public:
    CastAntiMagicShellAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "anti magic shell") {}
};

BEGIN_MELEE_SPELL_ACTION(CastAntiMagicZoneAction, "anti magic zone")
END_SPELL_ACTION()

// Chains of Ice follows the snare target (a fleeing creature, or the hostile player the bot is
// fighting once it starts moving); the kite wiring uses the current-target variant below.
class CastChainsOfIceAction : public CastSnareSpellAction
{
public:
    CastChainsOfIceAction(PlayerbotAI* botAI) : CastSnareSpellAction(botAI, "chains of ice") {}
};

class CastChainsOfIceOnTargetAction : public CastSpellAction
{
public:
    CastChainsOfIceOnTargetAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "chains of ice") {}
};

// Hungering Cold freezes everything around the death knight and needs no target of its own.
class CastHungeringColdAction : public CastBuffSpellAction
{
public:
    CastHungeringColdAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "hungering cold") {}
};

class CastHeartStrikeAction : public CastMeleeSpellAction
{
public:
    CastHeartStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "heart strike") {}
};

class CastBloodStrikeAction : public CastMeleeSpellAction
{
public:
    CastBloodStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "blood strike") {}
};

class CastFrostStrikeAction : public CastMeleeSpellAction
{
public:
    CastFrostStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "frost strike") {}
};

class CastObliterateAction : public CastMeleeSpellAction
{
public:
    CastObliterateAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "obliterate") {}
};

class CastDeathStrikeAction : public CastMeleeSpellAction
{
public:
    CastDeathStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "death strike") {}
};

class CastScourgeStrikeAction : public CastMeleeSpellAction
{
public:
    CastScourgeStrikeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "scourge strike") {}
};

class CastDeathCoilAction : public CastSpellAction
{
public:
    CastDeathCoilAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "death coil") {}
};

class CastBloodBoilAction : public CastMeleeSpellAction
{
public:
    CastBloodBoilAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "blood boil") {}
};

class CastDeathAndDecayAction : public CastSpellAction
{
public:
    CastDeathAndDecayAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "death and decay") {}
    // ActionThreatType getThreatType() override { return ActionThreatType::Aoe; }
};

class CastHornOfWinterAction : public CastSpellAction
{
public:
    CastHornOfWinterAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "horn of winter") {}
};

class CastImprovedIcyTalonsAction : public CastBuffSpellAction
{
public:
    CastImprovedIcyTalonsAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "improved icy talons") {}
};

class CastBoneShieldAction : public CastBuffSpellAction
{
public:
    CastBoneShieldAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "bone shield") {}
};

class CastDeathPactAction : public CastBuffSpellAction
{
public:
    CastDeathPactAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "death pact") {}
};

class CastDeathRuneMasteryAction : public CastBuffSpellAction
{
public:
    CastDeathRuneMasteryAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "death rune mastery") {}
};

class CastDancingRuneWeaponAction : public CastSpellAction
{
public:
    CastDancingRuneWeaponAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "dancing rune weapon") {}
};

class CastEmpowerRuneWeaponAction : public CastBuffSpellAction
{
public:
    CastEmpowerRuneWeaponAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "empower rune weapon") {}
};

// Army of the Dead is a 4.5 s channel. Against players it only pays off when someone else keeps
// the enemy off the bot for the duration - a group, or a gank by several enemies where eight ghouls
// are the best answer left. Solo against one player it would open (or end) every fight.
class CastArmyOfTheDeadAction : public CastBuffSpellAction
{
public:
    CastArmyOfTheDeadAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "army of the dead") {}
    bool isUseful() override;
};

class CastRaiseDeadAction : public CastBuffSpellAction
{
public:
    CastRaiseDeadAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "raise dead") {}
    virtual bool Execute(Event event) override;
};

class CastKillingMachineAction : public CastBuffSpellAction
{
public:
    CastKillingMachineAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "killing machine") {}
};

class CastIceboundFortitudeAction : public CastBuffSpellAction
{
public:
    CastIceboundFortitudeAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "icebound fortitude") {}
};

// Lichborne makes the death knight undead for ten seconds: the frost tree's answer to being
// feared, charmed or slept.
class CastLichborneAction : public CastBuffSpellAction
{
public:
    CastLichborneAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "lichborne") {}
};

class CastUnbreakableArmorAction : public CastBuffSpellAction
{
public:
    CastUnbreakableArmorAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "unbreakable armor") {}
};

class CastVampiricBloodAction : public CastBuffSpellAction
{
public:
    CastVampiricBloodAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "vampiric blood") {}
};

class CastMindFreezeAction : public CastMeleeSpellAction
{
public:
    CastMindFreezeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "mind freeze") {}
};

// Strangulate silences at 30 yards - the whole point is reaching a caster the bot cannot touch,
// so it must not be gated on melee range like Mind Freeze.
class CastStrangulateAction : public CastSpellAction
{
public:
    CastStrangulateAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "strangulate") {}
};

class CastStrangulateOnEnemyHealerAction : public CastSpellOnEnemyHealerAction
{
public:
    CastStrangulateOnEnemyHealerAction(PlayerbotAI* botAI) : CastSpellOnEnemyHealerAction(botAI, "strangulate") {}
};

class CastMindFreezeOnEnemyHealerAction : public CastSpellOnEnemyHealerAction
{
public:
    CastMindFreezeOnEnemyHealerAction(PlayerbotAI* botAI) : CastSpellOnEnemyHealerAction(botAI, "mind freeze") {}
};

class CastRuneTapAction : public CastMeleeSpellAction
{
public:
    CastRuneTapAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "rune tap") {}
};

class CastBloodTapAction : public CastMeleeSpellAction
{
public:
    CastBloodTapAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "blood tap") {}
};

class CastHysteriaAction : public CastSpellAction
{
public:
    CastHysteriaAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "hysteria") {}
    Unit* GetTarget() override;
};

#endif
