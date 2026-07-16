#ifndef PLAYERBOTS_WPVPACTIONS_H
#define PLAYERBOTS_WPVPACTIONS_H

#include "Action.h"

class PlayerbotAI;

// Face an unflagged enemy and throw a rude emote - stealthers drop stealth
// right next to the mark first. The bot is PvP-flagged, so if the mark takes
// the bait the regular reactive PvP strategy handles the fight.
class WpvpGoadAction : public Action
{
public:
    WpvpGoadAction(PlayerbotAI* botAI) : Action(botAI, "wpvp goad") {}

    bool Execute(Event event) override;
};

#endif
