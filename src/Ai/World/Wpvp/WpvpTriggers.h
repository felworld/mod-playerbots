#ifndef PLAYERBOTS_WPVPTRIGGERS_H
#define PLAYERBOTS_WPVPTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

// A stealthed rogue/druid on a world-PvP excursion is dwelling near an
// unflagged enemy it could provoke into flagging up.
class WpvpGoadTrigger : public Trigger
{
public:
    WpvpGoadTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp goad", 5) {}

    bool IsActive() override;

    static constexpr float GOAD_RANGE = 12.0f;
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
