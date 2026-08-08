/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "StealthReactActions.h"

#include "CellImpl.h"
#include "EmoteAction.h"
#include "FelworldEvents.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Metric.h"
#include "Playerbots.h"
#include "StealthReactValues.h"
#include "StringFormat.h"

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

bool FlushStealtherAction::isUseful()
{
    if (!bot->IsAlive() || bot->HasStealthAura())
        return false;

    // A perceivable enemy to fight outranks poking at shadows.
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive() && bot->CanSeeOrDetect(target))
        return false;

    StealthSuspicion suspicion = AI_VALUE(StealthSuspicion, "stealth suspicion");
    return suspicion.timeMs && suspicion.flushApproved;
}

bool FlushStealtherAction::Execute(Event /*event*/)
{
    StealthSuspicionValue* value =
        dynamic_cast<StealthSuspicionValue*>(context->GetValue<StealthSuspicion>("stealth suspicion"));
    if (!value)
        return false;

    StealthSuspicion suspicion = value->Get();
    if (!suspicion.timeMs)
        return false;

    Position const& spot = suspicion.lastKnown;
    float const dist = bot->GetExactDist(spot);
    char const* tool = nullptr;

    if (value->FlushCastReady())
    {
        switch (bot->getClass())
        {
            case CLASS_HUNTER:
            {
                uint32 flareId = AI_VALUE2(uint32, "spell id", "flare");
                if (flareId && dist <= 30.0f &&
                    botAI->CanCastSpell(flareId, spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ()) &&
                    botAI->CastSpell(flareId, spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ()))
                    tool = "flare";
                // 3.3.5a forbids placing traps in combat; out of it, mine
                // the spot on the way through.
                else if (!bot->IsInCombat() && dist < 8.0f && botAI->CanCastSpell("immolation trap", bot) &&
                         botAI->CastSpell("immolation trap", bot))
                    tool = "immolation trap";
                break;
            }
            case CLASS_PALADIN:
                if (dist < 8.0f && botAI->CanCastSpell("consecration", bot) && botAI->CastSpell("consecration", bot))
                    tool = "consecration";
                break;
            case CLASS_MAGE:
                if (dist < 10.0f && botAI->CanCastSpell("arcane explosion", bot) &&
                    botAI->CastSpell("arcane explosion", bot))
                    tool = "arcane explosion";
                break;
            case CLASS_PRIEST:
                if (dist < 10.0f && botAI->CanCastSpell("holy nova", bot) && botAI->CastSpell("holy nova", bot))
                    tool = "holy nova";
                break;
            case CLASS_DEATH_KNIGHT:
            {
                uint32 dndId = AI_VALUE2(uint32, "spell id", "death and decay");
                if (dndId && dist <= 30.0f &&
                    botAI->CanCastSpell(dndId, spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ()) &&
                    botAI->CastSpell(dndId, spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ()))
                    tool = "death and decay";
                break;
            }
            case CLASS_SHAMAN:
                if (dist < 8.0f && botAI->CanCastSpell("magma totem", bot) && botAI->CastSpell("magma totem", bot))
                    tool = "magma totem";
                break;
            default:
                break;
        }
    }

    if (tool)
    {
        value->MarkFlushCast();
        LOG_DEBUG("playerbots", "Bot {} sweeps for {} with {}", bot->GetName(), suspicion.stealtherName, tool);
        Felworld::LogEvent(
            bot->GetGUID(), "stealth_flush",
            Acore::StringFormat("{{\"suspect\":\"{}\",\"tool\":\"{}\"}}", suspicion.stealtherName, tool));
        METRIC_VALUE("playerbots_stealth_flush", 1, METRIC_TAG("tool", tool));
        return true;
    }

    // Nothing castable from here: search the spot like a player would.
    if (dist > 3.0f)
        return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                      false, false, MovementPriority::MOVEMENT_NORMAL);

    return false;
}
