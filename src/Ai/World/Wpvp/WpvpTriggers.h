#ifndef PLAYERBOTS_WPVPTRIGGERS_H
#define PLAYERBOTS_WPVPTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;
class Player;
class Unit;

// A bot dwelling on a world-PvP excursion is near an unflagged enemy it could
// provoke into flagging up. Rogues/druids goad from stealth (the reveal IS
// the provocation, so it waits until they're hidden and close); everyone else
// taunts openly from wherever the emote can be seen - unless shadowmelded,
// where staying hidden for the ambush beats taunting.
class WpvpGoadTrigger : public Trigger
{
public:
    WpvpGoadTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp goad", 5) {}

    bool IsActive() override;

    // Shared with WpvpGoadAction so the trigger and the action agree on the
    // mark.
    static Unit* FindMark(PlayerbotAI* botAI, Player* bot);

    static constexpr float STEALTH_GOAD_RANGE = 12.0f;
    static constexpr float OPEN_GOAD_RANGE = 25.0f;
};

// A Night Elf of a non-stealth class is dwelling on a world-PvP excursion and
// standing still: melt into the shadows. Mirror of the BG ShadowmeldTrigger
// without the battleground logic.
class WpvpShadowmeldTrigger : public Trigger
{
public:
    WpvpShadowmeldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp shadowmeld", 5) {}

    bool IsActive() override;
};

// A bored invader: dwelling on a world-PvP excursion past the boredom window
// with no enemy player anywhere in sight. One dice roll per excursion (made
// by the action) for the classic bait play - kill a guard or the flight
// master so the zone-under-attack alarm summons defenders to fight.
class WpvpRaidTrigger : public Trigger
{
public:
    WpvpRaidTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp raid", 10) {}

    bool IsActive() override;
};

// The bot just declined to start a fight because its own bars were below the
// WpvpInitiateSelf comfort thresholds (see WpvpReadiness) and the pass is
// still warm: sit down and fix them. The drink/food actions' own safety
// guards keep the bot from doing so within 40yd of an armed flagged enemy.
class WpvpDrinkUpTrigger : public Trigger
{
public:
    WpvpDrinkUpTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp drink up", 5) {}

    bool IsActive() override;
};

// While fighting one enemy player in the open world, another acceptable enemy
// has shown up at least AiPlayerbot.WpvpPeelAdvantageYards closer than the
// current fight - the point where a human would switch. "enemy player target"
// already resolves to the best alternative (it excludes the current victim),
// so firing "attack enemy player" performs the switch.
class WpvpPeelTrigger : public Trigger
{
public:
    WpvpPeelTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp peel", 3) {}

    bool IsActive() override;
};

#endif
