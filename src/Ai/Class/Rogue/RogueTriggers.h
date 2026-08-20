/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ROGUETRIGGERS_H
#define PLAYERBOTS_ROGUETRIGGERS_H

#include "GenericTriggers.h"

class PlayerbotAI;

class KickInterruptSpellTrigger : public InterruptSpellTrigger
{
public:
    KickInterruptSpellTrigger(PlayerbotAI* botAI) : InterruptSpellTrigger(botAI, "kick") {}
};

class SliceAndDiceTrigger : public BuffTrigger
{
public:
    SliceAndDiceTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "slice and dice") {}
};

class HungerForBloodTrigger : public BuffTrigger
{
public:
    HungerForBloodTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "hunger for blood") {}
};

class AdrenalineRushTrigger : public BoostTrigger
{
public:
    AdrenalineRushTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "adrenaline rush") {}

    // bool isPossible();
};

class BladeFlurryTrigger : public BoostTrigger
{
public:
    BladeFlurryTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "blade flurry") {}
};

class RuptureTrigger : public DebuffTrigger
{
public:
    RuptureTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "rupture", 1, true) {}
};

class ExposeArmorTrigger : public DebuffTrigger
{
public:
    ExposeArmorTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "expose armor") {}
    virtual bool IsActive() override;
};

class KickInterruptEnemyHealerSpellTrigger : public InterruptEnemyHealerTrigger
{
public:
    KickInterruptEnemyHealerSpellTrigger(PlayerbotAI* botAI) : InterruptEnemyHealerTrigger(botAI, "kick") {}
};

class InStealthTrigger : public HasAuraTrigger
{
public:
    InStealthTrigger(PlayerbotAI* botAI) : HasAuraTrigger(botAI, "stealth") {}
};

class NoStealthTrigger : public HasNoAuraTrigger
{
public:
    NoStealthTrigger(PlayerbotAI* botAI) : HasNoAuraTrigger(botAI, "stealth") {}
};

class UnstealthTrigger : public BuffTrigger
{
public:
    UnstealthTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "stealth", 3) {}

    bool IsActive() override;
};

class StealthTrigger : public Trigger
{
public:
    StealthTrigger(PlayerbotAI* botAI) : Trigger(botAI, "stealth") {}

    bool IsActive() override;
};

class SapTrigger : public HasCcTargetTrigger
{
public:
    SapTrigger(PlayerbotAI* botAI) : HasCcTargetTrigger(botAI, "sap") {}

    bool IsPossible();
};

// Kidney Shot is the rogue's PvP stun, not a damage finisher: spent on a player-controlled
// target that is not already locked down and not yet on the third diminishing-returns
// application, and only with enough combo points to be worth more than an Eviscerate.
class KidneyShotTrigger : public Trigger
{
public:
    KidneyShotTrigger(PlayerbotAI* botAI) : Trigger(botAI, "kidney shot", 1) {}

    bool IsActive() override;
};

// Dismantle takes the weapons off a melee player for ten seconds; useless against a caster and
// against anything that is not player-controlled.
class DismantleTrigger : public Trigger
{
public:
    DismantleTrigger(PlayerbotAI* botAI) : Trigger(botAI, "dismantle", 1) {}

    bool IsActive() override;
};

// Deadly Throw is wired through the shared snare-target value, which prefers the hostile player
// the bot is fighting and otherwise picks a runner.
class DeadlyThrowSnareTrigger : public SnareTargetTrigger
{
public:
    DeadlyThrowSnareTrigger(PlayerbotAI* botAI) : SnareTargetTrigger(botAI, "deadly throw") {}
};

// The stealth opener a rogue actually uses: with a second hostile player standing next to the
// mark and the fight not started yet, Sap them so the opener lands one-on-one. The shared "cc
// target" value cannot produce this - it only scans units already attacking the bot, and Sap
// needs a target still out of combat.
class SapOpenerTrigger : public Trigger
{
public:
    SapOpenerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "sap opener", 1) {}

    bool IsActive() override;
};

class SprintTrigger : public BuffTrigger
{
public:
    SprintTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "sprint", 3) {}

    bool IsPossible();
    bool IsActive() override;
};

class MainHandWeaponNoEnchantTrigger : public BuffTrigger
{
public:
    MainHandWeaponNoEnchantTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "main hand", 1) {}
    virtual bool IsActive();
};

class OffHandWeaponNoEnchantTrigger : public BuffTrigger
{
public:
    OffHandWeaponNoEnchantTrigger(PlayerbotAI* ai) : BuffTrigger(ai, "off hand", 1) {}
    virtual bool IsActive();
};

// The off-hand carries Deadly Poison in PvE, but against players the Crippling snare is what
// keeps a caster or a healer inside melee range and is worth far more than the damage over time.
class OffHandWeaponNoEnchantPvpTrigger : public OffHandWeaponNoEnchantTrigger
{
public:
    OffHandWeaponNoEnchantPvpTrigger(PlayerbotAI* ai) : OffHandWeaponNoEnchantTrigger(ai) {}

    bool IsActive() override;
};

class TricksOfTheTradeOnMainTankTrigger : public BuffOnMainTankTrigger
{
public:
    TricksOfTheTradeOnMainTankTrigger(PlayerbotAI* ai) : BuffOnMainTankTrigger(ai, "tricks of the trade", true) {}
};

// The Distract opener trick: stealthed in front of a watching player,
// turn them away and take the straight line to their back instead of
// circling (StealthFlankAction's walk-in branch handles the rest). Only
// rogues that know the trick use it - a stable per-character roll against
// RogueDistractChance - and only when it's worth a cast: full energy,
// a target that cons yellow or above, and no area effect already down
// that would sweep the approach regardless of facing.
class DistractTrigger : public Trigger
{
public:
    DistractTrigger(PlayerbotAI* botAI) : Trigger(botAI, "distract", 1) {}

    bool IsActive() override;
};

#endif
