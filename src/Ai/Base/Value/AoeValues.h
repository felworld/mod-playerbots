/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AOEVALUES_H
#define PLAYERBOTS_AOEVALUES_H

#include "AiObjectContext.h"
#include "GameObject.h"
#include "Object.h"
#include "Value.h"

class PlayerbotAI;

class AoePositionValue : public CalculatedValue<WorldLocation>
{
public:
    AoePositionValue(PlayerbotAI* botAI) : CalculatedValue<WorldLocation>(botAI, "aoe position") {}

    WorldLocation Calculate() override;
};

class AoeCountValue : public CalculatedValue<uint8>
{
public:
    AoeCountValue(PlayerbotAI* botAI) : CalculatedValue<uint8>(botAI, "aoe count") {}

    uint8 Calculate() override;
};

class HasAreaDebuffValue : public BoolCalculatedValue, public Qualified
{
public:
    HasAreaDebuffValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI) {}

    Unit* GetTarget()
    {
        AiObjectContext* ctx = AiObject::context;

        return ctx->GetValue<Unit*>(qualifier)->Get();
    }
    virtual bool Calculate();
};

class AreaDebuffValue : public CalculatedValue<Aura*>
{
public:
    AreaDebuffValue(PlayerbotAI* botAI) : CalculatedValue<Aura*>(botAI, "area debuff", 1) {}

    Aura* Calculate() override;
};

// Standing on the exact edge of a cloud is one step away from standing back in it, and the flee
// destination is measured from the bot's own feet rather than from the cloud's centre - a few yards
// of slack is what turns "out of the fire" into "staying out of it" (Felworld).
constexpr float AOE_DYNOBJ_SAFETY_MARGIN = 3.0f;

// Hostile ground hazards (Maraudon's Noxious Cloud, Rain of Fire, ...) are DynamicObjects carrying a
// persistent area aura. Leaving one has to key off the object rather than off the aura it has
// already landed on the bot, which is what "area debuff" does: that aura re-targets only every
// 500ms, never lands at all on an immune bot, and the cloud outlives its caster's death, because
// dynamic objects are torn down in Unit::RemoveFromWorld and not in Unit::setDeathState. Cached like
// "nearest trap with damage" - without the cache this grid search would run every tick (Felworld).
class NearestDamagingDynObjectsValue : public ObjectGuidListCalculatedValue
{
public:
    NearestDamagingDynObjectsValue(PlayerbotAI* botAI)
        : ObjectGuidListCalculatedValue(botAI, "nearest damaging dynamic objects", 1 * 1000)
    {
    }

protected:
    GuidVector Calculate() override;
};

#endif
