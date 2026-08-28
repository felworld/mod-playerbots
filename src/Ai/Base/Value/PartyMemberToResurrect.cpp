/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PartyMemberToResurrect.h"
#include "Group.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

class IsTargetOfResurrectSpell : public SpellEntryPredicate
{
public:
    bool Check(SpellInfo const* spellInfo) override
    {
        for (uint8 i = 0; i < 3; ++i)
        {
            if (spellInfo->Effects[i].Effect == SPELL_EFFECT_RESURRECT ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_RESURRECT_NEW ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_SELF_RESURRECT)
                return true;
        }

        return false;
    }
};

class FindDeadPlayer : public FindPlayerPredicate
{
public:
    FindDeadPlayer(PartyMemberValue* value) : value(value) {}

    bool Check(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        return player && !player->isResurrectRequested() && player->getDeathState() == DeathState::Corpse &&
               !value->IsTargetOfSpellCast(player, predicate);
    }

private:
    PartyMemberValue* value;
    IsTargetOfResurrectSpell predicate;
};

Unit* PartyMemberToResurrect::Calculate()
{
    FindDeadPlayer finder(this);
    return FindPartyMember(finder);
}

namespace
{
    bool IsResurrecterClass(uint8 cls)
    {
        return cls == CLASS_PRIEST || cls == CLASS_PALADIN || cls == CLASS_SHAMAN || cls == CLASS_DRUID;
    }

    // Spellbook scan instead of a class/level table: a level-9 priest has no
    // Resurrection yet, and custom content stays covered. Self-resurrects
    // (soulstones) don't count - they can't recover anyone else.
    bool KnowsResurrectSpell(Player* player)
    {
        for (auto const& [spellId, playerSpell] : player->GetSpellMap())
        {
            if (playerSpell->State == PLAYERSPELL_REMOVED || !playerSpell->Active)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!spellInfo || spellInfo->IsPassive())
                continue;

            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                if (spellInfo->Effects[i].Effect == SPELL_EFFECT_RESURRECT ||
                    spellInfo->Effects[i].Effect == SPELL_EFFECT_RESURRECT_NEW)
                    return true;
        }

        return false;
    }

    bool GroupHasLiveResurrecter(Player* bot)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsInWorld() || !member->IsAlive())
                continue;

            // A rezzer on another map is not coming; cables stay on the table.
            if (member->GetMapId() != bot->GetMapId())
                continue;

            if (IsResurrecterClass(member->getClass()) && KnowsResurrectSpell(member))
                return true;
        }

        return false;
    }
}

class FindDeadResurrecter : public FindDeadPlayer
{
public:
    using FindDeadPlayer::FindDeadPlayer;

    bool Check(Unit* unit) override
    {
        return FindDeadPlayer::Check(unit) && KnowsResurrectSpell(unit->ToPlayer());
    }
};

Unit* PartyMemberToJumperCable::Calculate()
{
    // A living rezzer makes cables redundant - don't race the healer to the
    // corpse for a coin-flip when a real resurrection is a cast away.
    if (GroupHasLiveResurrecter(bot))
        return nullptr;

    FindDeadResurrecter deadResurrecter(this);
    if (Unit* target = FindPartyMember(deadResurrecter))
        return target;

    // No rezzers in the group at all (or their corpses are out of reach):
    // jump-starting anyone is still the only play there is.
    FindDeadPlayer anyone(this);
    return FindPartyMember(anyone);
}
