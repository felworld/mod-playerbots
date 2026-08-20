/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MageActions.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "SharedDefines.h"
#include "UseItemAction.h"
#include <cmath>

namespace
{
// Worth spending the point-blank root on: alive, not already frozen, and not a mob the template
// says is immune to the freeze mechanic.
bool IsWorthFreezing(Unit* unit)
{
    if (!unit || !unit->IsAlive() || !unit->IsInWorld() || unit->isFrozen())
        return false;

    Creature* creature = unit->ToCreature();
    return !creature || !creature->HasMechanicTemplateImmunity(1 << (MECHANIC_FREEZE - 1));
}
}

std::vector<NextAction> CastMoltenArmorAction::getAlternatives()
{
    if (!botAI->HasSpell("molten armor"))
        return NextAction::merge({ NextAction("mage armor") }, CastBuffSpellAction::getAlternatives());

    return CastBuffSpellAction::getAlternatives();
}

std::vector<NextAction> CastMageArmorAction::getAlternatives()
{
    if (!botAI->HasSpell("mage armor"))
        return NextAction::merge({ NextAction("ice armor") }, CastBuffSpellAction::getAlternatives());

    return CastBuffSpellAction::getAlternatives();
}

bool UseManaSapphireAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(33312, false) > 0;  // Mana Sapphire
}

bool UseManaEmeraldAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(22044, false) > 0;  // Mana Emerald
}

bool UseManaRubyAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(8008, false) > 0;  // Mana Ruby
}

bool UseManaCitrineAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(8007, false) > 0;  // Mana Citrine
}

bool UseManaJadeAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(5513, false) > 0;  // Mana Jade
}

bool UseManaAgateAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(5514, false) > 0;  // Mana Agate
}

bool CastFrostNovaAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (IsWorthFreezing(target) && bot->IsWithinCombatRange(target, 10.f))
        return true;

    // Frost Nova is point-blank, so whoever closed to melee on us is in range even while we are
    // casting at something else - that is the case a mage actually presses it in.
    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    for (ObjectGuid const guid : attackers)
    {
        Unit* attacker = botAI->GetUnit(guid);
        if (!IsWorthFreezing(attacker) || attacker->GetVictim() != bot)
            continue;

        if (bot->IsWithinCombatRange(attacker, 10.f))
            return true;
    }

    return false;
}

bool CastConeOfColdAction::isUseful()
{
    bool facingTarget = AI_VALUE2(bool, "facing", "current target");
    bool targetClose = ServerFacade::instance().IsDistanceLessOrEqualThan(
        AI_VALUE2(float, "distance", GetTargetName()), 10.f);

    return facingTarget && targetClose;
}

bool CastDragonsBreathAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    bool facingTarget = AI_VALUE2(bool, "facing", "current target");
    bool targetClose = bot->IsWithinCombatRange(target, 10.0f);
    return facingTarget && targetClose;
}

bool CastBlastWaveAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    bool targetClose = bot->IsWithinCombatRange(target, 10.0f);
    return targetClose;
}

Unit* CastFocusMagicOnPartyAction::GetTarget()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Unit* casterDps = nullptr;
    Unit* healer = nullptr;
    Unit* target = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            bot->GetDistance(member) > sPlayerbotAIConfig.spellDistance || member->HasAura(54646))  // Focus Magic
        {
            continue;
        }

        if (member->getClass() == CLASS_MAGE)
            return member;

        if (!casterDps && botAI->IsCaster(member) && botAI->IsDps(member))
            casterDps = member;

        if (!healer && botAI->IsHeal(member))
            healer = member;

        if (!target)
            target = member;
    }

    if (casterDps)
        return casterDps;

    if (healer)
        return healer;

    return target;
}

bool CastBlinkBackAction::Execute(Event event)
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    bot->SetOrientation(bot->GetAngle(target) + M_PI);
    return CastSpellAction::Execute(event);
}

Value<Unit*>* CastPolymorphOnTargetAction::GetTargetValue() { return context->GetValue<Unit*>("current target"); }

Value<Unit*>* CastPyroblastOnCcTargetAction::GetTargetValue()
{
    return context->GetValue<Unit*>("current cc target", "polymorph");
}
