/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PRIESTTRIGGERS_H
#define PLAYERBOTS_PRIESTTRIGGERS_H

#include "CureTriggers.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include <set>

class PlayerbotAI;

DEBUFF_CHECKISOWNER_TRIGGER(HolyFireTrigger, "holy fire");
DEBUFF_CHECKISOWNER_TRIGGER(ShadowWordPainTrigger, "shadow word: pain");
DEBUFF_ENEMY_TRIGGER(ShadowWordPainOnAttackerTrigger, "shadow word: pain");
DEBUFF_CHECKISOWNER_TRIGGER(VampiricTouchTrigger, "vampiric touch");
DEBUFF_ENEMY_TRIGGER(VampiricTouchOnAttackerTrigger, "vampiric touch on attacker");
BUFF_TRIGGER(VampiricEmbraceTrigger, "vampiric embrace");
CURE_TRIGGER(DispelMagicTrigger, "dispel magic", DISPEL_MAGIC);
CURE_PARTY_TRIGGER(DispelMagicPartyMemberTrigger, "dispel magic", DISPEL_MAGIC);
CURE_TRIGGER(CureDiseaseTrigger, "cure disease", DISPEL_DISEASE);
CURE_PARTY_TRIGGER(PartyMemberCureDiseaseTrigger, "cure disease", DISPEL_DISEASE);
BUFF_TRIGGER_A(InnerFireTrigger, "inner fire");
BUFF_TRIGGER_A(ShadowformTrigger, "shadowform");
BOOST_TRIGGER(PowerInfusionTrigger, "power infusion");
BUFF_TRIGGER(InnerFocusTrigger, "inner focus");
CC_TRIGGER(ShackleUndeadTrigger, "shackle undead");
INTERRUPT_TRIGGER(SilenceTrigger, "silence");
INTERRUPT_HEALER_TRIGGER(SilenceEnemyHealerTrigger, "silence");

// racials
DEBUFF_CHECKISOWNER_TRIGGER(DevouringPlagueTrigger, "devouring plague");
BUFF_TRIGGER(TouchOfWeaknessTrigger, "touch of weakness");
DEBUFF_TRIGGER(HexOfWeaknessTrigger, "hex of weakness");
BUFF_TRIGGER(ShadowguardTrigger, "shadowguard");
BUFF_TRIGGER(FearWardTrigger, "fear ward");
DEFLECT_TRIGGER(FeedbackTrigger, "feedback");
SNARE_TRIGGER(ChastiseTrigger, "chastise");

BOOST_TRIGGER_A(ShadowfiendTrigger, "shadowfiend");

// Psychic Scream is the priest's answer to a melee opponent, but it breaks on damage and would
// scatter a mob pack across the party, so it is held for a player-controlled target that has
// closed to melee - exactly when a human priest screams to get their casting distance back.
class PsychicScreamTrigger : public Trigger
{
public:
    PsychicScreamTrigger(PlayerbotAI* botAI) : Trigger(botAI, "psychic scream", 2) {}

    bool IsActive() override;
};

// Inner Fire lasts half an hour, so only the non-combat engine ever refreshed it and a priest
// who lost it to a dispel fought the rest of the fight without it. Shadowform is excluded: the
// combat action node drops the form first, which costs the rotation more than the armour is worth.
class InnerFireCombatTrigger : public InnerFireTrigger
{
public:
    InnerFireCombatTrigger(PlayerbotAI* botAI) : InnerFireTrigger(botAI) {}

    bool IsActive() override;
};

// Fear Ward counters the fear that opens most fights a priest loses (warlock Fear and Howl of
// Terror, another priest's Psychic Scream, a warrior's Intimidating Shout). The non-combat
// engine keeps it up; in combat it is only worth a global against players, and only once the
// ward has actually been eaten.
class FearWardPvpTrigger : public BuffTrigger
{
public:
    FearWardPvpTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "fear ward", 5) {}

    bool IsActive() override;
};

class ShadowProtectionTrigger : public BuffTrigger
{
public:
    ShadowProtectionTrigger(PlayerbotAI* botAI)
    : BuffTrigger(botAI, "shadow protection", 4 * 2000) {}

    bool IsActive() override;
};

class ShadowProtectionOnPartyTrigger : public BuffOnPartyTrigger
{
public:
    ShadowProtectionOnPartyTrigger(PlayerbotAI* botAI)
        : BuffOnPartyTrigger(botAI, "shadow protection", 4 * 2000) {}
};

class PowerWordFortitudeOnPartyTrigger : public BuffOnPartyTrigger
{
public:
    PowerWordFortitudeOnPartyTrigger(PlayerbotAI* botAI)
        : BuffOnPartyTrigger(botAI, "power word: fortitude", 4 * 2000) {}
};

class PowerWordFortitudeTrigger : public BuffTrigger
{
public:
    PowerWordFortitudeTrigger(PlayerbotAI* botAI)
        : BuffTrigger(botAI, "power word: fortitude", 4 * 2000) {}

    bool IsActive() override;
};

class DivineSpiritOnPartyTrigger : public BuffOnPartyTrigger
{
public:
    DivineSpiritOnPartyTrigger(PlayerbotAI* botAI)
        : BuffOnPartyTrigger(botAI, "divine spirit", 4 * 2000) {}
};

class DivineSpiritTrigger : public BuffTrigger
{
public:
    DivineSpiritTrigger(PlayerbotAI* botAI)
        : BuffTrigger(botAI, "divine spirit", 4 * 2000) {}

    bool IsActive() override;
};

class BindingHealTrigger : public PartyMemberLowHealthTrigger
{
public:
    BindingHealTrigger(PlayerbotAI* botAI);

    bool IsActive() override;
};

class MindSearChannelCheckTrigger : public Trigger
{
public:
    MindSearChannelCheckTrigger(PlayerbotAI* botAI, uint32 minEnemies = 2)
        : Trigger(botAI, "mind sear channel check"), minEnemies(minEnemies) {}

    bool IsActive() override;

protected:
    uint32 minEnemies;
    static const std::set<uint32> MIND_SEAR_SPELL_IDS;
};

#endif
