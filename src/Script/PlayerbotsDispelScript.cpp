/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DispelBackoff.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellInfo.h"

// Feeds the dispel-backoff board (see DispelBackoff.h): whenever an enemy
// dispels a playerbot's debuff, record it so the bot stops reapplying
// debuffs of that school into a cleanse loop.
class PlayerbotsDispelScript : public UnitScript
{
public:
    PlayerbotsDispelScript() : UnitScript("PlayerbotsDispelScript", true, { UNITHOOK_ON_AURA_REMOVE }) {}

    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode mode) override
    {
        // AURA_REMOVE_BY_ENEMY_SPELL is the enemy actively stripping the
        // aura (dispel, purge, spellsteal, mass dispel) - the only removals
        // worth learning from. Expiry, death and overwrites stay invisible.
        if (mode != AURA_REMOVE_BY_ENEMY_SPELL)
            return;

        if (!sPlayerbotAIConfig.debuffDispelBackoffCount)
            return;

        Aura const* aura = aurApp->GetBase();
        SpellInfo const* spellInfo = aura->GetSpellInfo();

        // Only debuffs the bot put on an opponent; a purged buff on an ally
        // is a different story and would only pollute the board.
        if (spellInfo->IsPositive())
            return;

        ObjectGuid const casterGuid = aura->GetCasterGUID();
        if (!casterGuid.IsPlayer() || casterGuid == unit->GetGUID())
            return;

        Unit* casterUnit = ObjectAccessor::GetUnit(*unit, casterGuid);
        Player* caster = casterUnit ? casterUnit->ToPlayer() : nullptr;
        if (!caster || !GET_PLAYERBOT_AI(caster))
            return;

        DispelBackoffBoard::instance().RecordDispel(caster, unit, spellInfo->Dispel);
    }
};

void AddPlayerbotsDispelScripts()
{
    new PlayerbotsDispelScript();
}
