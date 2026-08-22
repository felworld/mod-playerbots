/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_IMMUNITYVALUES_H
#define PLAYERBOTS_IMMUNITYVALUES_H

#include "Value.h"

#include <ctime>
#include <string>

class Aura;
class PlayerbotAI;

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
