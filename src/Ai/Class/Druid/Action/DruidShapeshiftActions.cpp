/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DruidShapeshiftActions.h"
#include "Playerbots.h"

bool CastBearFormAction::isUseful()
{
    return CastBuffSpellAction::isUseful() && !botAI->HasAura("dire bear form", GetTarget());
}

bool CastBearFormAction::isPossible()
{
    return CastBuffSpellAction::isPossible() && !botAI->HasAura("dire bear form", GetTarget());
}

std::vector<NextAction> CastDireBearFormAction::getAlternatives()
{
    return NextAction::merge({NextAction("bear form")}, CastSpellAction::getAlternatives());
}

bool CastTravelFormAction::isUseful()
{
    bool firstmount = bot->GetLevel() >= 20;

    // useful if no mount or with wsg flag
    return !bot->IsMounted() && (!firstmount || (bot->HasAura(23333) || bot->HasAura(23335) || bot->HasAura(34976))) &&
           !botAI->HasAura("dash", bot);
}

bool CastCasterFormAction::Execute(Event /*event*/)
{
    botAI->RemoveShapeshift();
    return true;
}

bool CastCasterFormAction::isUseful()
{
    return botAI->HasAnyAuraOf(GetTarget(), "dire bear form", "bear form", "cat form", "travel form", "aquatic form",
                               "flight form", "swift flight form", "moonkin form", nullptr) &&
           AI_VALUE2(uint8, "mana", "self target") > sPlayerbotAIConfig.mediumHealth;
}

bool CastCasterFormForCureAction::isUseful()
{
    uint32 form = bot->GetShapeshiftForm();
    if (!form)
        return false;

    static std::vector<std::string> const cureSpells = { "remove curse", "abolish poison", "cure poison" };
    for (std::string const& cureSpell : cureSpells)
    {
        uint32 spellId = AI_VALUE2(uint32, "spell id", cureSpell);
        if (!spellId)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (spellInfo && spellInfo->CheckShapeshift(form) != SPELL_CAST_OK)
            return true;
    }

    return false;
}

bool CastCancelDruidAction::Execute(Event /*event*/)
{
    botAI->RemoveAura(auraName);
    return true;
}

bool CastCancelDruidAction::isUseful() { return bot->HasAura(auraId); }

bool CastTreeFormAction::isUseful()
{
    constexpr uint32 SPELL_TREE_OF_LIFE = 33891;
    return GetTarget() && CastSpellAction::isUseful() && !bot->HasAura(SPELL_TREE_OF_LIFE);
}
