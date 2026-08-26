/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AoeValues.h"
#include "CellImpl.h"
#include "DynamicObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

GuidVector FindMaxDensity(Player* bot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    GuidVector units = *botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets");

    std::map<ObjectGuid, GuidVector> groups;
    uint32 maxCount = 0;
    ObjectGuid maxGroup;
    for (GuidVector::iterator i = units.begin(); i != units.end(); ++i)
    {
        Unit* unit = botAI->GetUnit(*i);
        if (!unit)
            continue;

        for (GuidVector::iterator j = units.begin(); j != units.end(); ++j)
        {
            Unit* other = botAI->GetUnit(*j);
            if (!other)
                continue;

            float d = ServerFacade::instance().GetDistance2d(unit, other);
            if (ServerFacade::instance().IsDistanceLessOrEqualThan(d, sPlayerbotAIConfig.aoeRadius * 2))
                groups[*i].push_back(*j);
        }

        if (maxCount < groups[*i].size())
        {
            maxCount = groups[*i].size();
            maxGroup = *i;
        }
    }

    if (!maxCount)
        return GuidVector();

    return groups[maxGroup];
}

WorldLocation AoePositionValue::Calculate()
{
    GuidVector group = FindMaxDensity(bot);
    if (group.empty())
        return WorldLocation();

    // Note: don't know where these values come from or even used.
    float x1 = 0.f;
    float y1 = 0.f;
    float x2 = 0.f;
    float y2 = 0.f;
    for (GuidVector::iterator i = group.begin(); i != group.end(); ++i)
    {
        Unit* unit = GET_PLAYERBOT_AI(bot)->GetUnit(*i);
        if (!unit)
            continue;

        if (i == group.begin() || x1 > unit->GetPositionX())
            x1 = unit->GetPositionX();

        if (i == group.begin() || x2 < unit->GetPositionX())
            x2 = unit->GetPositionX();

        if (i == group.begin() || y1 > unit->GetPositionY())
            y1 = unit->GetPositionY();

        if (i == group.begin() || y2 < unit->GetPositionY())
            y2 = unit->GetPositionY();
    }

    float x = (x1 + x2) / 2;
    float y = (y1 + y2) / 2;
    float z = bot->GetPositionZ() + CONTACT_DISTANCE;
    ;
    bot->UpdateAllowedPositionZ(x, y, z);
    return WorldLocation(bot->GetMapId(), x, y, z, 0);
}

uint8 AoeCountValue::Calculate() { return FindMaxDensity(bot).size(); }

bool HasAreaDebuffValue::Calculate()
{
    for (uint32 auraType = SPELL_AURA_BIND_SIGHT; auraType < TOTAL_AURAS; auraType++)
    {
        Unit::AuraEffectList const& auras = botAI->GetBot()->GetAuraEffectsByType((AuraType)auraType);

        for (AuraEffect const* aurEff : auras)
        {
            SpellInfo const* proto = aurEff->GetSpellInfo();

            if (!proto)
                continue;

            uint32 trigger_spell_id = proto->Effects[aurEff->GetEffIndex()].TriggerSpell;
            if (trigger_spell_id == 29767)  // Overload
                return true;

            if (!proto->IsPositive() && aurEff->IsPeriodic() && proto->HasAreaAuraEffect())
                return true;
        }
    }

    return false;
}

Aura* AreaDebuffValue::Calculate()
{
    // Unit::AuraApplicationMap& map = bot->GetAppliedAuras();
    Unit::AuraEffectList const& aurasPeriodicDamage = bot->GetAuraEffectsByType(SPELL_AURA_PERIODIC_DAMAGE);
    Unit::AuraEffectList const& aurasPeriodicDamagePercent =
        bot->GetAuraEffectsByType(SPELL_AURA_PERIODIC_DAMAGE_PERCENT);
    Unit::AuraEffectList const& aurasPeriodicTriggerSpell =
        bot->GetAuraEffectsByType(SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    Unit::AuraEffectList const& aurasPeriodicTriggerWithValueSpell =
        bot->GetAuraEffectsByType(SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE);
    Unit::AuraEffectList const& aurasDummy = bot->GetAuraEffectsByType(SPELL_AURA_DUMMY);
    for (const Unit::AuraEffectList& list : {aurasPeriodicDamage, aurasPeriodicDamagePercent, aurasPeriodicTriggerSpell,
                                             aurasPeriodicTriggerWithValueSpell, aurasDummy})
    {
        for (auto i = list.begin(); i != list.end(); ++i)
        {
            AuraEffect* aurEff = *i;
            if (!aurEff)
                continue;
            Aura* aura = aurEff->GetBase();
            if (!aura)
                continue;
            AuraObjectType type = aura->GetType();
            bool isPositive = aura->GetSpellInfo()->IsPositive();
            if (type == DYNOBJ_AURA_TYPE && !isPositive)
            {
                DynamicObject* dynOwner = aura->GetDynobjOwner();
                if (!dynOwner)
                {
                    continue;
                }
                // float radius = dynOwner->GetRadius();
                // if (radius > 12.0f)
                //     continue;
                return aura;
            }
        }
    }
    return nullptr;
}

namespace
{
    bool SpellDealsDamage(SpellInfo const* spellInfo)
    {
        if (!spellInfo)
            return false;

        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (spellInfo->Effects[i].Effect == SPELL_EFFECT_SCHOOL_DAMAGE)
                return true;

            switch (spellInfo->Effects[i].ApplyAuraName)
            {
                case SPELL_AURA_PERIODIC_DAMAGE:
                case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
                case SPELL_AURA_PERIODIC_LEECH:
                    return true;
                default:
                    break;
            }
        }

        return false;
    }

    // The damage of a ground hazard sits either on the persistent area aura itself (Noxious Cloud)
    // or on the spell that aura periodically triggers (Rain of Fire). A hostile field that only
    // snares or silences is not worth scattering the party over, so the trigger is followed through.
    bool IsHarmfulAreaAura(SpellInfo const* spellInfo)
    {
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (spellInfo->Effects[i].Effect != SPELL_EFFECT_PERSISTENT_AREA_AURA)
                continue;

            switch (spellInfo->Effects[i].ApplyAuraName)
            {
                case SPELL_AURA_PERIODIC_DAMAGE:
                case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
                case SPELL_AURA_PERIODIC_LEECH:
                    return true;
                case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
                case SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE:
                    if (SpellDealsDamage(sSpellMgr->GetSpellInfo(spellInfo->Effects[i].TriggerSpell)))
                        return true;
                    break;
                default:
                    break;
            }
        }

        return false;
    }
}  // namespace

GuidVector NearestDamagingDynObjectsValue::Calculate()
{
    float const range = sPlayerbotAIConfig.maxAoeAvoidRadius + AOE_DYNOBJ_SAFETY_MARGIN;

    std::list<WorldObject*> objects;
    Acore::AllWorldObjectsInRange check(bot, range);
    Acore::WorldObjectListSearcher<Acore::AllWorldObjectsInRange> searcher(bot, objects, check,
                                                                          GRID_MAP_TYPE_MASK_DYNAMICOBJECT);
    Cell::VisitObjects(bot, searcher, range);

    GuidVector result;
    for (WorldObject* object : objects)
    {
        DynamicObject* dynObj = object->ToDynObject();
        if (!dynObj)
            continue;

        // Same guard the other detectors use: a hazard wider than the bot can reasonably run out of
        // is a fight mechanic to be healed through, not dodged.
        float const radius = dynObj->GetRadius();
        if (radius <= 0.0f || radius > sPlayerbotAIConfig.maxAoeAvoidRadius)
            continue;

        // A friendly ground effect (Consecration, Healing Stream) is a place to stand, not to leave.
        // A caster that has already left the world leaves its cloud behind, and that still hurts.
        Unit* caster = dynObj->GetCaster();
        if (caster && caster->IsFriendlyTo(bot))
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(dynObj->GetSpellId());
        if (!spellInfo || spellInfo->IsPositive() || !IsHarmfulAreaAura(spellInfo))
            continue;

        if (sPlayerbotAIConfig.aoeAvoidSpellWhitelist.find(spellInfo->Id) !=
            sPlayerbotAIConfig.aoeAvoidSpellWhitelist.end())
            continue;

        result.push_back(dynObj->GetGUID());
    }

    return result;
}
