/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_SOCIALBUFFVALUES_H
#define _PLAYERBOT_SOCIALBUFFVALUES_H

#include "ObjectGuid.h"
#include "Timer.h"
#include "Value.h"

#include <string>
#include <unordered_map>

class Player;
class PlayerbotAI;
class Unit;

// The class buff this bot would give the target right now, or "" - empty when
// the bot's class has no buff to share, the target already carries it (or its
// group variant), or the bot can't cast it (unknown, no mana, out of range).
std::string SelectSocialBuffFor(PlayerbotAI* botAI, Player* bot, Unit* target);

// Casting a beneficial spell on a PvP-flagged unit flags the caster
// (Spell::DoAllEffectOnTarget) - never let an unflagged bot do it.
bool CanBuffWithoutFlagging(Player* bot, Unit* target);

// A reaction owed to another player, stashed by the social UnitScript hooks
// (OnAuraApply/OnHeal) for the AI loop to act on. timeMs of 0 means "none".
struct SocialReactionEvent
{
    ObjectGuid caster;
    uint32 timeMs = 0;
};

// How long a stashed reaction stays actionable. Buffing back only reads
// naturally in the moment; a thank-you keeps until after the fight the heal
// arrived in.
constexpr uint32 SOCIAL_BUFF_BACK_WINDOW_MS = 30 * 1000;
constexpr uint32 SOCIAL_THANK_WINDOW_MS = 60 * 1000;

inline bool IsSocialReactionFresh(SocialReactionEvent const& event, uint32 windowMs)
{
    return event.timeMs && getMSTimeDiff(event.timeMs, getMSTime()) < windowMs;
}

// The nearby friendly player this bot should walk-up buff, or nullptr. Owns
// the per-target cooldown that keeps a bot from courting the same stranger
// every scan.
class PasserbyToBuffValue : public UnitCalculatedValue
{
public:
    PasserbyToBuffValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "passerby to buff") {}

    Unit* Calculate() override;

    void MarkBuffed(ObjectGuid targetGuid);

private:
    std::unordered_map<ObjectGuid, uint32> _cooldownEndMs;
    uint32 _lastPruneMs = 0;
};

#endif
