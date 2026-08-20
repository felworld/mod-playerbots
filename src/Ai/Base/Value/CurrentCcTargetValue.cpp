/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "CurrentCcTargetValue.h"
#include "AiObjectContext.h"
#include "PlayerbotAI.h"

class FindCurrentCcTargetStrategy : public FindTargetStrategy
{
public:
    FindCurrentCcTargetStrategy(PlayerbotAI* botAI, std::string const spell) : FindTargetStrategy(botAI), spell(spell)
    {
    }

    void CheckAttacker(Unit* attacker, ThreatManager* /*threatMgr*/) override
    {
        if (botAI->HasAura(spell, attacker))
            result = attacker;
    }

private:
    std::string const spell;
};

Unit* CurrentCcTargetValue::Calculate()
{
    FindCurrentCcTargetStrategy strategy(botAI, qualifier);
    if (Unit* target = FindTarget(&strategy))
        return target;

    // A unit under damage-breakable CC leaves "attackers" (AttackersValue::IsCrowdControlled), so
    // the unit we just controlled has to be found among everything hostile nearby - otherwise the
    // CC trigger sees "no current cc target" and casts again on someone else, which for
    // single-target CC like Polymorph would release the first one.
    GuidVector possible = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get();
    for (ObjectGuid const guid : possible)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && botAI->HasAura(qualifier, unit))
            return unit;
    }

    return nullptr;
}
