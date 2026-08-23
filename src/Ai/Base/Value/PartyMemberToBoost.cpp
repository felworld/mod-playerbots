/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PartyMemberToBoost.h"
#include "AttackersValue.h"
#include "Playerbots.h"

namespace
{
constexpr uint32 SPELL_POWER_INFUSION = 10060;
constexpr uint32 SPELL_BLOODLUST = 2825;
constexpr uint32 SPELL_HEROISM = 32182;

// Power Infusion does not stack with Bloodlust or Heroism, and a second copy of itself is
// simply thrown away.
bool HasHasteCooldown(Unit* unit)
{
    return unit->HasAura(SPELL_POWER_INFUSION) || unit->HasAura(SPELL_BLOODLUST) || unit->HasAura(SPELL_HEROISM);
}

class BoostablePartyMemberPredicate : public FindPlayerPredicate
{
public:
    bool Check(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player || !player->IsAlive() || !player->IsInCombat())
            return false;

        // Both halves of the buff - the haste and the mana discount - are lost on a class that
        // does not cast from a mana bar.
        if (player->getPowerType() != POWER_MANA)
            return false;

        if (!PlayerbotAI::IsCaster(player) || !PlayerbotAI::IsDps(player))
            return false;

        if (HasHasteCooldown(player))
            return false;

        // A sheeped or silenced mage casts nothing for the length of the buff: the classic way
        // to waste Power Infusion.
        return !AttackersValue::IsCrowdControlled(player) && !player->HasUnitState(UNIT_STATE_CONTROLLED) &&
               !player->HasAuraType(SPELL_AURA_MOD_SILENCE) && !player->HasAuraType(SPELL_AURA_MOD_PACIFY_SILENCE);
    }
};
}  // namespace

Unit* PartyMemberToBoost::Calculate()
{
    BoostablePartyMemberPredicate predicate;
    return FindPartyMember(predicate);
}

bool PartyMemberToBoost::Check(Unit* unit)
{
    return PartyMemberValue::Check(unit) && bot->GetDistance(unit) <= sPlayerbotAIConfig.spellDistance;
}
