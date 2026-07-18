/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "SocialBuffActions.h"

#include "EmoteAction.h"
#include "Playerbots.h"
#include "SocialBuffValues.h"

namespace
{
    void MarkBuffed(AiObjectContext* context, ObjectGuid targetGuid)
    {
        if (PasserbyToBuffValue* value =
                dynamic_cast<PasserbyToBuffValue*>(context->GetValue<Unit*>("passerby to buff")))
            value->MarkBuffed(targetGuid);
    }

    // Shared by the walk-up buff and the buff-back: drop druid forms first
    // (returning true to retry next tick), then cast the class buff.
    bool CastSocialBuff(PlayerbotAI* botAI, Player* bot, AiObjectContext* context, Unit* target)
    {
        if (bot->getClass() == CLASS_DRUID && bot->HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
        {
            bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
            return true;
        }

        std::string const spell = SelectSocialBuffFor(botAI, bot, target);
        if (spell.empty())
            return false;

        if (!botAI->CastSpell(spell, target))
            return false;

        MarkBuffed(context, target->GetGUID());
        return true;
    }

    // Take the pending reaction and clear it: whatever happens next, a
    // reaction is only ever attempted once.
    SocialReactionEvent TakePending(AiObjectContext* context, char const* name)
    {
        Value<SocialReactionEvent>* value = context->GetValue<SocialReactionEvent>(name);
        SocialReactionEvent event = value->Get();
        value->Set(SocialReactionEvent());
        return event;
    }
}

bool BuffPasserbyAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive())
        return false;

    if (!bot->IsWithinDistInMap(target, botAI->GetRange("spell")))
        return false;

    // The value already vetted this, but state can change between the
    // trigger and the cast - never let an unflagged bot flag itself.
    return CanBuffWithoutFlagging(bot, target);
}

bool BuffPasserbyAction::Execute(Event /*event*/)
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    return CastSocialBuff(botAI, bot, context, target);
}

bool BuffBackAction::isUseful()
{
    return IsSocialReactionFresh(AI_VALUE(SocialReactionEvent, "pending buff back"), SOCIAL_BUFF_BACK_WINDOW_MS);
}

bool BuffBackAction::Execute(Event /*event*/)
{
    SocialReactionEvent event = TakePending(context, "pending buff back");
    if (!event.timeMs)
        return false;

    Unit* caster = botAI->GetUnit(event.caster);
    if (!caster || !caster->IsAlive() || !caster->IsFriendlyTo(bot))
        return false;

    // Reactions don't chase: if the buffer has already wandered off, let it go.
    if (!bot->IsWithinDistInMap(caster, botAI->GetRange("spell")))
        return false;

    if (!CanBuffWithoutFlagging(bot, caster))
        return false;

    return CastSocialBuff(botAI, bot, context, caster);
}

bool ThankHealerAction::isUseful()
{
    return IsSocialReactionFresh(AI_VALUE(SocialReactionEvent, "pending thank"), SOCIAL_THANK_WINDOW_MS);
}

bool ThankHealerAction::Execute(Event /*event*/)
{
    SocialReactionEvent event = TakePending(context, "pending thank");
    if (!event.timeMs)
        return false;

    Unit* healer = botAI->GetUnit(event.caster);
    if (!healer || !healer->IsAlive())
        return false;

    if (!bot->IsWithinDistInMap(healer, sPlayerbotAIConfig.sightDistance))
        return false;

    bot->SetFacingToObject(healer);

    // A targeted /thank: "%s thanks %s." plus the voice line, like PlayEmote
    // but aimed at the healer instead of master/target.
    WorldPacket data(SMSG_TEXT_EMOTE);
    data << uint32(TEXT_EMOTE_THANK);
    data << EmoteAction::GetNumberOfEmoteVariants(TEXT_EMOTE_THANK, bot->getRace(), bot->getGender());
    data << healer->GetGUID();
    bot->GetSession()->HandleTextEmoteOpcode(data);

    return true;
}
