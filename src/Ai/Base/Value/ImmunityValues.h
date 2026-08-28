/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_IMMUNITYVALUES_H
#define PLAYERBOTS_IMMUNITYVALUES_H

#include "ObjectGuid.h"
#include "Value.h"

#include <ctime>
#include <string>

class Aura;
class PlayerbotAI;
class Unit;

// The bot's last own immunity cast (Ice Block, Divine Shield, Dispersion): which trigger asked for
// it and when. "cancel immunity" only manages an aura it can match to a survival cast this way; an
// immunity cast for any other reason is left alone (Felworld).
struct ImmunityCast
{
    std::string trigger;
    time_t castTime = 0;

    // Whether this record belongs to the given aura instance, not an earlier or later cast.
    bool Matches(Aura const* aura) const;
};

class ImmunityCastValue : public ManualSetValue<ImmunityCast>
{
public:
    ImmunityCastValue(PlayerbotAI* botAI) : ManualSetValue<ImmunityCast>(botAI, ImmunityCast(), "immunity cast") {}
};

// The mob the bot last put its own Banish on. Recorded on the cast rather than looked up, because a
// banished mob is immune to everything and so leaves every target list the bot keeps - there is
// nowhere left to find it. Held as a GUID and re-checked against the aura at use time, so an
// interrupted cast, a dead mob or a banish that has already run out reads as "nothing banished"
// (Felworld).
class BanishedTargetValue : public ManualSetValue<ObjectGuid>
{
public:
    BanishedTargetValue(PlayerbotAI* botAI) : ManualSetValue<ObjectGuid>(botAI, ObjectGuid::Empty, "banished target")
    {
    }
};

// A Banish of the bot's own that is still up: the mob, and the rank that landed on it. The rank
// matters because re-casting is what releases it and the core only drops an aura of the very spell
// re-cast (spell_warl_banish), so a warlock that dinged into rank 2 mid-fight still has to release
// its rank 1. Both empty when there is nothing to release (Felworld).
struct BanishedTarget
{
    Unit* unit = nullptr;
    uint32 spellId = 0;

    explicit operator bool() const { return unit != nullptr; }
};

BanishedTarget FindBanishedTarget(PlayerbotAI* botAI);

// Dropping an immunity would not get the bot killed: out of combat, the fight is over (nothing
// alive is fighting the bot: last mob dead, duel ended), or healed back up with nothing waiting to
// resume attacking it - no mob that would come straight back (the core suppresses immune victims
// on the threat list, so "who is attacking me" reads empty under the aura; the threat list says
// who returns), no enemy player in sight. A tank wants the mobs back and only needs the health
// (Felworld).
class SafeToDropImmunityValue : public BoolCalculatedValue
{
public:
    SafeToDropImmunityValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "safe to drop immunity") {}

    bool Calculate() override;

private:
    bool FightIsOver();
    bool MobWouldReturn();
    bool EnemyPlayerInSight();
};

#endif
