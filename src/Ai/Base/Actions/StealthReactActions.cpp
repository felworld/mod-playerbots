/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "StealthReactActions.h"

#include "CellImpl.h"
#include "EmoteAction.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Playerbots.h"
#include "StealthReactValues.h"

namespace
{
    // How long the bot stays frozen after the snap-turn before resuming
    // whatever it was doing - long enough to read, short enough not to
    // look stunned.
    constexpr uint32 STARTLE_PAUSE_MS = 1500;

    // How far around the bot other players count as an audience for the
    // follow-up emote decisions.
    constexpr float EMOTE_AUDIENCE_RANGE = 40.0f;

    void MarkReacted(AiObjectContext* context, ObjectGuid stealtherGuid)
    {
        if (StealtherSpottedValue* value =
                dynamic_cast<StealtherSpottedValue*>(context->GetValue<Unit*>("stealther spotted")))
            value->MarkReacted(stealtherGuid);
    }

    // Take the pending emote and clear it: attempted once, whatever happens.
    StealthSpotEvent TakePending(AiObjectContext* context)
    {
        Value<StealthSpotEvent>* value = context->GetValue<StealthSpotEvent>("pending stealth emote");
        StealthSpotEvent event = value->Get();
        value->Set(StealthSpotEvent());
        return event;
    }

    std::list<Player*> AudienceAround(Player* bot)
    {
        std::list<Player*> players;
        Acore::AnyPlayerInObjectRangeCheck check(bot, EMOTE_AUDIENCE_RANGE, /*reqAlive*/ true, /*disallowGM*/ true);
        Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(bot, players, check);
        Cell::VisitObjects(bot, searcher, EMOTE_AUDIENCE_RANGE);
        return players;
    }
}

bool StartleAtStealtherAction::isUseful()
{
    Unit* target = GetTarget();
    return target && target->IsAlive() && target->HasStealthAura();
}

bool StartleAtStealtherAction::Execute(Event /*event*/)
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    MarkReacted(context, target->GetGUID());

    // The involuntary part, mirroring CreatureAI::TriggerAlert: freeze
    // mid-stride, whip around to face them, hold still for a beat.
    bot->ClearUnitState(UNIT_STATE_CHASE);
    bot->ClearUnitState(UNIT_STATE_FOLLOW);
    if (bot->isMoving())
        bot->StopMoving();
    bot->SetFacingToObject(target);
    botAI->SetNextCheckDelay(STARTLE_PAUSE_MS);

    // Sometimes the fright resolves into an emote once the pause ends.
    if (roll_chance_i(sPlayerbotAIConfig.stealthReactionEmoteChance))
        context->GetValue<StealthSpotEvent>("pending stealth emote")
            ->Set(StealthSpotEvent{ target->GetGUID(), getMSTime() });

    return true;
}

bool StealthSpotEmoteAction::isUseful()
{
    StealthSpotEvent event = AI_VALUE(StealthSpotEvent, "pending stealth emote");
    return event.timeMs && getMSTimeDiff(event.timeMs, getMSTime()) < STEALTH_SPOT_EMOTE_WINDOW_MS;
}

bool StealthSpotEmoteAction::Execute(Event /*event*/)
{
    StealthSpotEvent event = TakePending(context);
    if (!event.timeMs)
        return false;

    Unit* stealther = botAI->GetUnit(event.stealther);
    if (!stealther || !stealther->IsAlive() || !stealther->HasStealthAura())
        return false;

    if (!bot->IsWithinDistInMap(stealther, MAX_PLAYER_STEALTH_DETECT_RANGE))
        return false;

    uint32 emote;
    if (bot->IsFriendlyTo(stealther))
    {
        // A wave tells everyone watching exactly where the sneak is - only
        // offer it when no enemy is around to take advantage.
        for (Player* other : AudienceAround(bot))
            if (other->IsHostileTo(bot))
                return false;

        emote = TEXT_EMOTE_WAVE;
    }
    else
    {
        // Calling out an enemy stealther is only worth anything when a
        // friendly is around to hear it...
        bool haveFriendlyAudience = false;
        for (Player* other : AudienceAround(bot))
        {
            if (other == bot || other->GetGUID() == event.stealther)
                continue;

            if (other->IsFriendlyTo(bot))
            {
                haveFriendlyAudience = true;
                break;
            }
        }

        if (!haveFriendlyAudience)
            return false;

        // ...and pointing instead of fighting only makes sense while the
        // bot isn't in fight mode itself.
        if (bot->IsPvP() || bot->IsFFAPvP())
            return false;

        emote = TEXT_EMOTE_POINT;
    }

    bot->SetFacingToObject(stealther);

    // A targeted "/wave" or "/point": "%s waves at %s." plus the voice
    // line, aimed at the stealther like ThankHealerAction does.
    WorldPacket data(SMSG_TEXT_EMOTE);
    data << uint32(emote);
    data << EmoteAction::GetNumberOfEmoteVariants(TextEmotes(emote), bot->getRace(), bot->getGender());
    data << stealther->GetGUID();
    bot->GetSession()->HandleTextEmoteOpcode(data);

    return true;
}
