/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_STEALTHREACTACTIONS_H
#define _PLAYERBOT_STEALTHREACTACTIONS_H

#include "Action.h"
#include "MovementActions.h"

class PlayerbotAI;
class Unit;

// The involuntary "oh crap" moment when the bot's stealth detection pings
// someone sneaking nearby: freeze mid-stride, snap around to face them,
// hold for a beat. Sometimes followed by a social emote - a /wave at a
// friendly sneak (only when no enemy could use it to find them), or a
// /point calling out a hostile one (only when a friendly is around to
// warn and the bot isn't in fight mode itself).

class StartleAtStealtherAction : public Action
{
public:
    StartleAtStealtherAction(PlayerbotAI* botAI) : Action(botAI, "startle at stealther") {}

    std::string const GetTargetName() override { return "stealther spotted"; }
    bool Execute(Event event) override;
    bool isUseful() override;
};

class StealthSpotEmoteAction : public Action
{
public:
    StealthSpotEmoteAction(PlayerbotAI* botAI) : Action(botAI, "stealth spot emote") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Sweep the last-known spot of an enemy the bot believes went into hiding:
// walk over like a player searching, and flush with whatever the class
// has - Consecration, Flare on the spot, a trap, Arcane Explosion, Holy
// Nova, Death and Decay, Magma Totem. Classes without a tool still search
// the spot. Never used against a directly targetable stealther - once the
// bot perceives them again the suspicion clears and ordinary targeting
// (and a druid's Faerie Fire) takes over.
class FlushStealtherAction : public MovementAction
{
public:
    FlushStealtherAction(PlayerbotAI* botAI) : MovementAction(botAI, "flush stealther") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// The counter to the rogue Distract trick: a bot that sees through the
// noise breaks the forced facing and spins to the opposite bearing - the
// rogue threw it to get the bot's back, so that's where the rogue is -
// then plants a suspicion on the inferred approach lane for the flush
// machinery above to sweep. The bot never learns who threw it: everything
// here derives from its own forced facing, and the suspicion carries no
// identity.
class ShakeOffDistractAction : public Action
{
public:
    ShakeOffDistractAction(PlayerbotAI* botAI) : Action(botAI, "shake off distract") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
