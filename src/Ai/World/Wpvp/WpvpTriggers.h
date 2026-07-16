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

#endif
