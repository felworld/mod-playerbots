/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PriestActions.h"
#include "Event.h"
#include "FelworldEvents.h"
#include "ImmunitySpells.h"
#include "Metric.h"
#include "Playerbots.h"
#include "StringFormat.h"

bool CastRemoveShadowformAction::Execute(Event /*event*/)
{
    botAI->RemoveAura("shadowform");
    return true;
}

bool CastRemoveShadowformAction::isUseful() { return botAI->HasAura("shadowform", AI_VALUE(Unit*, "self target")); }

Unit* CastPowerWordShieldOnAlmostFullHealthBelowAction::GetTarget()
{
    Group* group = bot->GetGroup();
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player)
            continue;
        if (player->isDead())
        {
            continue;
        }
        if (player->GetHealthPct() > sPlayerbotAIConfig.almostFullHealth)
        {
            continue;
        }
        if (player->GetDistance2d(bot) > sPlayerbotAIConfig.spellDistance)
        {
            continue;
        }
        if (botAI->HasAnyAuraOf(player, "weakened soul", "power word: shield", nullptr))
        {
            continue;
        }
        return player;
    }
    return nullptr;
}

bool CastPowerWordShieldOnAlmostFullHealthBelowAction::isUseful()
{
    Group* group = bot->GetGroup();
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player)
            continue;
        if (player->isDead())
        {
            continue;
        }
        if (player->GetHealthPct() > sPlayerbotAIConfig.almostFullHealth)
        {
            continue;
        }
        if (player->GetDistance2d(bot) > sPlayerbotAIConfig.spellDistance)
        {
            continue;
        }
        if (botAI->HasAnyAuraOf(player, "weakened soul", "power word: shield", nullptr))
        {
            continue;
        }
        return true;
    }
    return false;
}

Unit* CastPowerWordShieldOnNotFullAction::GetTarget()
{
    Group* group = bot->GetGroup();
    MinValueCalculator calc(100);
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player)
            continue;
        if (player->isDead() || player->IsFullHealth())
        {
            continue;
        }
        if (player->GetDistance2d(bot) > sPlayerbotAIConfig.spellDistance)
        {
            continue;
        }
        if (botAI->HasAnyAuraOf(player, "weakened soul", "power word: shield", nullptr))
        {
            continue;
        }
        calc.probe(player->GetHealthPct(), player);
    }
    return (Unit*)calc.param;
}

bool CastPowerWordShieldOnNotFullAction::isUseful()
{
    return GetTarget();
}

Value<Unit*>* CastPowerInfusionOnPartyAction::GetTargetValue()
{
    return context->GetValue<Unit*>("party member to boost");
}

bool CastMassDispelAction::isUseful()
{
    // "spell id" reads the bot's own spellbook - zero until the priest has learned Mass Dispel.
    if (!AI_VALUE2(uint32, "spell id", "mass dispel"))
        return false;

    // The spell is a third of base mana - a priest that would be left dry is better off with the
    // plain standoff (backing away and drinking) one relevance step below.
    if (AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.mediumMana)
        return false;

    Unit* enemy = AI_VALUE(Unit*, "immune enemy near");
    return enemy && ai::immunity::HasDispellableImmunity(enemy) &&
           bot->IsWithinDistInMap(enemy, botAI->GetRange("spell"));
}

bool CastMassDispelAction::Execute(Event /*event*/)
{
    Unit* enemy = AI_VALUE(Unit*, "immune enemy near");
    if (!enemy || !ai::immunity::HasDispellableImmunity(enemy))
        return false;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", "mass dispel");
    if (!spellId)
        return false;

    // Cast at the enemy's position: the unit-target cast path refuses immune targets, the dest
    // path has no such gate, and stripping through the bubble is Mass Dispel's own privilege
    // (SPELL_ATTR0_UNAFFECTED_BY_INVULNERABILITY in its spell data).
    float x = enemy->GetPositionX();
    float y = enemy->GetPositionY();
    float z = enemy->GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);

    if (!botAI->CanCastSpell(spellId, x, y, z) || !botAI->CastSpell(spellId, x, y, z))
        return false;

    LOG_DEBUG("playerbots", "Bot {} mass dispels the immunity off {}", bot->GetName(), enemy->GetName());
    Felworld::LogEvent(bot->GetGUID(), "mass_dispel_immunity",
                       Acore::StringFormat("{{\"target\":\"{}\"}}", enemy->GetName()));
    METRIC_VALUE("playerbots_mass_dispel_immunity", 1);
    return true;
}
