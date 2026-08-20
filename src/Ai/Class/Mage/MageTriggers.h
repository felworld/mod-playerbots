/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_MAGETRIGGERS_H
#define PLAYERBOTS_MAGETRIGGERS_H

#include "CureTriggers.h"
#include "GenericTriggers.h"
#include "Playerbots.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include <unordered_set>

// Buff and Out of Combat Triggers

class ArcaneIntellectOnPartyTrigger : public BuffOnPartyTrigger
{
public:
    ArcaneIntellectOnPartyTrigger(PlayerbotAI* botAI)
        : BuffOnPartyTrigger(botAI, "arcane intellect", 4 * 2000) {}
};

class ArcaneIntellectTrigger : public BuffTrigger
{
public:
    ArcaneIntellectTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "arcane intellect", 4 * 2000) {}
    bool IsActive() override;
};

class MageArmorTrigger : public BuffTrigger
{
public:
    MageArmorTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "mage armor", 5 * 2000) {}
    bool IsActive() override;
};

class MoltenArmorTrigger : public BuffTrigger
{
public:
    MoltenArmorTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "molten armor", 5 * 2000) {}
    bool IsActive() override;
};

class NoFocusMagicTrigger : public Trigger
{
public:
    NoFocusMagicTrigger(PlayerbotAI* botAI) : Trigger(botAI, "no focus magic") {}
    bool IsActive() override;
};

class IceBarrierTrigger : public BuffTrigger
{
public:
    IceBarrierTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "ice barrier") {}
};

class NoManaGemTrigger : public Trigger
{
public:
    NoManaGemTrigger(PlayerbotAI* botAI) : Trigger(botAI, "no mana gem") {}
    bool IsActive() override;
};

class FireWardTrigger : public DeflectSpellTrigger
{
public:
    FireWardTrigger(PlayerbotAI* botAI) : DeflectSpellTrigger(botAI, "fire ward") {}
};

class FrostWardTrigger : public DeflectSpellTrigger
{
public:
    FrostWardTrigger(PlayerbotAI* botAI) : DeflectSpellTrigger(botAI, "frost ward") {}
};

// Proc and Boost Triggers

class HotStreakTrigger : public HasAuraTrigger
{
public:
    HotStreakTrigger(PlayerbotAI* botAI) : HasAuraTrigger(botAI, "hot streak") {}
};

class FirestarterTrigger : public HasAuraTrigger
{
public:
    FirestarterTrigger(PlayerbotAI* botAI) : HasAuraTrigger(botAI, "firestarter") {}
};

class MissileBarrageTrigger : public HasAuraTrigger
{
public:
    MissileBarrageTrigger(PlayerbotAI* botAI) : HasAuraTrigger(botAI, "missile barrage") {}
};

class ArcaneBlastTrigger : public BuffTrigger
{
public:
    ArcaneBlastTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "arcane blast") {}
};

class ArcaneBlastStackTrigger : public HasAuraStackTrigger
{
public:
    ArcaneBlastStackTrigger(PlayerbotAI* botAI) : HasAuraStackTrigger(botAI, "arcane blast", 4, 1) {}
};

class ArcaneBlast4StacksAndMissileBarrageTrigger : public TwoTriggers
{
public:
    ArcaneBlast4StacksAndMissileBarrageTrigger(PlayerbotAI* botAI)
        : TwoTriggers(botAI, "arcane blast stack", "missile barrage") {}
};

class CombustionTrigger : public BoostTrigger
{
public:
    CombustionTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "combustion") {}
};

class IcyVeinsCooldownTrigger : public SpellCooldownTrigger
{
public:
    IcyVeinsCooldownTrigger(PlayerbotAI* botAI) : SpellCooldownTrigger(botAI, "icy veins") {}
};

class DeepFreezeCooldownTrigger : public SpellCooldownTrigger
{
public:
    DeepFreezeCooldownTrigger(PlayerbotAI* botAI) : SpellCooldownTrigger(botAI, "deep freeze") {}

    bool IsActive() override;
};

class ColdSnapTrigger : public TwoTriggers
{
public:
    ColdSnapTrigger(PlayerbotAI* botAI) : TwoTriggers(botAI, "icy veins on cd", "deep freeze on cd") {}
};

class MirrorImageTrigger : public BoostTrigger
{
public:
    MirrorImageTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "mirror image") {}
};

class IcyVeinsTrigger : public BoostTrigger
{
public:
    IcyVeinsTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "icy veins") {}
};

class ArcanePowerTrigger : public BoostTrigger
{
public:
    ArcanePowerTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "arcane power") {}
};
class PresenceOfMindTrigger : public BoostTrigger
{
public:
    PresenceOfMindTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "presence of mind") {}
};

// Presence of Mind is up and waiting to be spent. The next spell with a cast time consumes it,
// so the spec's biggest nuke has to outrank the rotation for exactly this window. The charge has
// no timer, which rules out HasAuraTrigger (it discards auras of unlimited duration).
class PresenceOfMindActiveTrigger : public Trigger
{
public:
    PresenceOfMindActiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, "presence of mind active") {}

    bool IsActive() override { return botAI->HasAura("presence of mind", bot); }
};

// CC, Interrupt, and Dispel Triggers

class PolymorphTrigger : public HasCcTargetTrigger
{
public:
    PolymorphTrigger(PlayerbotAI* botAI) : HasCcTargetTrigger(botAI, "polymorph") {}
};

// The setup a human mage opens a duel or a world-PvP fight with: sheep the player we just
// engaged, then land a full Pyroblast on them. Only an even 1v1 from full health qualifies -
// with an ally or a second attacker around, Polymorph belongs on an add instead (the shared
// "cc target" machinery already handles that case).
class PolymorphOpenerTrigger : public Trigger
{
public:
    PolymorphOpenerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "polymorph opener", 1000) {}

    bool IsActive() override;
};

// Our Polymorph is on the one player we are fighting: the Pyroblast that breaks it is the whole
// point of the opener. Anyone else still swinging at us means the sheep is an add we put away
// deliberately, and breaking it would be a mistake.
class PolymorphedOpponentTrigger : public Trigger
{
public:
    PolymorphedOpponentTrigger(PlayerbotAI* botAI) : Trigger(botAI, "polymorphed opponent") {}

    bool IsActive() override;
};

// Slow keeps a hostile player at arm's length and stretches their casts; no lifetime gate, a
// player about to die is still worth slowing.
class SlowKiteTrigger : public DebuffTrigger
{
public:
    SlowKiteTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "slow", 1, true, 0.0f) {}

    bool IsActive() override;
};

class RemoveCurseTrigger : public NeedCureTrigger
{
public:
    RemoveCurseTrigger(PlayerbotAI* botAI) : NeedCureTrigger(botAI, "remove curse", DISPEL_CURSE) {}
};

class PartyMemberRemoveCurseTrigger : public PartyMemberNeedCureTrigger
{
public:
    PartyMemberRemoveCurseTrigger(PlayerbotAI* botAI)
        : PartyMemberNeedCureTrigger(botAI, "remove curse", DISPEL_CURSE) {}
};

class SpellstealTrigger : public TargetAuraDispelTrigger
{
public:
    SpellstealTrigger(PlayerbotAI* botAI) : TargetAuraDispelTrigger(botAI, "spellsteal", DISPEL_MAGIC) {}
};

class CounterspellEnemyHealerTrigger : public InterruptEnemyHealerTrigger
{
public:
    CounterspellEnemyHealerTrigger(PlayerbotAI* botAI) : InterruptEnemyHealerTrigger(botAI, "counterspell") {}
};

class CounterspellInterruptSpellTrigger : public InterruptSpellTrigger
{
public:
    CounterspellInterruptSpellTrigger(PlayerbotAI* botAI) : InterruptSpellTrigger(botAI, "counterspell") {}
};

// Damage and Debuff Triggers

class LivingBombTrigger : public DebuffTrigger
{
public:
    LivingBombTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "living bomb", 1, true) {}
    bool IsActive() override { return BuffTrigger::IsActive(); }
};

class LivingBombOnAttackersTrigger : public DebuffOnAttackerTrigger
{
public:
    LivingBombOnAttackersTrigger(PlayerbotAI* botAI) : DebuffOnAttackerTrigger(botAI, "living bomb", true) {}
    bool IsActive() override { return BuffTrigger::IsActive(); }
};

class FireballTrigger : public DebuffTrigger
{
public:
    FireballTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "fireball", 1, true) {}
};

class ImprovedScorchTrigger : public DebuffTrigger
{
public:
    ImprovedScorchTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "improved scorch", 1, true, 0.5f) {}
    bool IsActive() override;
};

class PyroblastTrigger : public DebuffTrigger
{
public:
    PyroblastTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "pyroblast", 1, true) {}
};

class FrostfireBoltTrigger : public DebuffTrigger
{
public:
    FrostfireBoltTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "frostfire bolt", 1, true) {}
};

class FingersOfFrostTrigger : public HasAuraTrigger
{
public:
    FingersOfFrostTrigger(PlayerbotAI* botAI) : HasAuraTrigger(botAI, "fingers of frost") {}
};

class BrainFreezeTrigger : public HasAuraTrigger
{
public:
    BrainFreezeTrigger(PlayerbotAI* botAI) : HasAuraTrigger(botAI, "fireball!") {}
};

class FrostNovaOnTargetTrigger : public DebuffTrigger
{
public:
    FrostNovaOnTargetTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "frost nova", 1, false) {}
    bool IsActive() override;
};

class FrostbiteOnTargetTrigger : public DebuffTrigger
{
public:
    FrostbiteOnTargetTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "frostbite", 1, false) {}
    bool IsActive() override;
};

// Frost Nova is a point-blank root, so what matters is whether anything actually hitting us is
// inside its radius - the current target can be a caster thirty yards away while a rogue is on
// our back.
class MeleeAttackerInNovaRangeTrigger : public Trigger
{
public:
    MeleeAttackerInNovaRangeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "melee attacker in nova range") {}

    bool IsActive() override;
};

class FlamestrikeNearbyTrigger : public Trigger
{
public:
    FlamestrikeNearbyTrigger(PlayerbotAI* botAI, float radius = 30.0f)
        : Trigger(botAI, "flamestrike nearby"), radius(radius)
    {
    }
    bool IsActive() override;

protected:
    float radius;
    static const std::unordered_set<uint32> FLAMESTRIKE_SPELL_IDS;
};

class FlamestrikeBlizzardTrigger : public TwoTriggers
{
public:
    FlamestrikeBlizzardTrigger(PlayerbotAI* botAI) : TwoTriggers(botAI, "flamestrike nearby", "medium aoe") {}
};

class BlizzardChannelCheckTrigger : public Trigger
{
public:
    BlizzardChannelCheckTrigger(PlayerbotAI* botAI, uint32 minEnemies = 2)
        : Trigger(botAI, "blizzard channel check"), minEnemies(minEnemies) {}

    bool IsActive() override;

protected:
    uint32 minEnemies;
    static const std::unordered_set<uint32> BLIZZARD_SPELL_IDS;
};

class BlastWaveOffCdTrigger : public SpellNoCooldownTrigger
{
public:
    BlastWaveOffCdTrigger(PlayerbotAI* botAI) : SpellNoCooldownTrigger(botAI, "blast wave") {}
};

class BlastWaveOffCdTriggerAndMediumAoeTrigger : public TwoTriggers
{
public:
    BlastWaveOffCdTriggerAndMediumAoeTrigger(PlayerbotAI* botAI)
        : TwoTriggers(botAI, "blast wave off cd", "medium aoe") {}
};

class NoFirestarterStrategyTrigger : public Trigger
{
public:
    NoFirestarterStrategyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "no firestarter strategy") {}

    bool IsActive() override
    {
        return !botAI->HasStrategy("firestarter", BOT_STATE_COMBAT);
    }
};

class EnemyIsCloseAndNoFirestarterStrategyTrigger : public TwoTriggers
{
public:
    EnemyIsCloseAndNoFirestarterStrategyTrigger(PlayerbotAI* botAI)
        : TwoTriggers(botAI, "enemy is close", "no firestarter strategy") {}
};

class EnemyTooCloseForSpellAndNoFirestarterStrategyTrigger : public TwoTriggers
{
public:
    EnemyTooCloseForSpellAndNoFirestarterStrategyTrigger(PlayerbotAI* botAI)
        : TwoTriggers(botAI, "enemy too close for spell", "no firestarter strategy") {}
};

class MeleeAttackerInNovaRangeAndNoFirestarterStrategyTrigger : public TwoTriggers
{
public:
    MeleeAttackerInNovaRangeAndNoFirestarterStrategyTrigger(PlayerbotAI* botAI)
        : TwoTriggers(botAI, "melee attacker in nova range", "no firestarter strategy") {}
};

#endif
