/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "BystanderAssistActions.h"

#include "BystanderValues.h"
#include "Playerbots.h"

namespace
{
    // Ordered fast-to-slow; unknown or too-low-level spells simply fail
    // CanCastSpell, so one list per class covers every level.
    std::vector<std::string> const* HealSpellsFor(Player* bot)
    {
        static std::vector<std::string> const priest = { "flash heal", "heal", "lesser heal" };
        static std::vector<std::string> const druid = { "regrowth", "healing touch", "rejuvenation" };
        static std::vector<std::string> const shaman = { "lesser healing wave", "healing wave" };
        static std::vector<std::string> const paladin = { "flash of light", "holy light" };

        switch (bot->getClass())
        {
            case CLASS_PRIEST:
                return &priest;
            case CLASS_DRUID:
                return &druid;
            case CLASS_SHAMAN:
                return &shaman;
            case CLASS_PALADIN:
                return &paladin;
            default:
                return nullptr;
        }
    }

    void MarkAssisted(AiObjectContext* context, Unit* victim)
    {
        if (!victim)
            return;

        if (BystanderToAssistValue* value =
                dynamic_cast<BystanderToAssistValue*>(context->GetValue<Unit*>("bystander to assist")))
            value->MarkAssisted(victim->GetGUID());
    }
}

bool BystanderHealAction::isUseful()
{
    if (!IsBystanderHealerClass(bot))
        return false;

    Unit* victim = GetTarget();
    if (!victim || !victim->IsAlive())
        return false;

    if (!bot->IsWithinDistInMap(victim, botAI->GetRange("heal")))
        return false;

    // The value already vetted this, but state can change between the
    // trigger and the cast - never let an unflagged bot flag itself.
    return bot->IsPvP() || (!victim->IsPvP() && !victim->HasUnitState(UNIT_STATE_ATTACK_PLAYER));
}

bool BystanderHealAction::Execute(Event /*event*/)
{
    Unit* victim = GetTarget();
    if (!victim)
        return false;

    // A shapeshifted druid can't heal; drop form now, heal on the next tick
    // (the victim stays selected - MarkAssisted hasn't run yet).
    if (bot->getClass() == CLASS_DRUID && bot->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
    {
        bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
        return true;
    }

    std::vector<std::string> const* spells = HealSpellsFor(bot);
    if (!spells)
        return false;

    for (std::string const& spell : *spells)
    {
        if (!botAI->CanCastSpell(spell, victim))
            continue;

        if (botAI->CastSpell(spell, victim))
        {
            MarkAssisted(context, victim);
            return true;
        }
    }

    return false;
}

ReachBystanderToAssistAction::ReachBystanderToAssistAction(PlayerbotAI* botAI)
    : ReachTargetAction(botAI, "reach bystander to assist", botAI->GetRange("heal"))
{
}

bool ReachBystanderToAssistAction::isUseful()
{
    // Only healers close the gap on foot; other classes charge the attacker
    // instead, and AttackAction brings its own movement.
    return IsBystanderHealerClass(bot) && ReachTargetAction::isUseful();
}

bool AttackBystanderAttackerAction::isUseful()
{
    if (IsBystanderHealerClass(bot))
        return false;

    return context->GetValue<Unit*>("bystander attacker")->Get() != nullptr;
}

bool AttackBystanderAttackerAction::Execute(Event event)
{
    Unit* victim = AI_VALUE(Unit*, "bystander to assist");

    if (!AttackAction::Execute(event))
        return false;

    MarkAssisted(context, victim);
    return true;
}
