#ifndef PLAYERBOTS_WPVPACTIONS_H
#define PLAYERBOTS_WPVPACTIONS_H

#include "Action.h"

class PlayerbotAI;

// Drop stealth right next to an unflagged enemy, face them and throw a rude
// emote. The bot is PvP-flagged, so if the mark takes the bait the regular
// reactive PvP strategy handles the fight.
class WpvpGoadAction : public Action
{
public:
    WpvpGoadAction(PlayerbotAI* botAI) : Action(botAI, "wpvp goad") {}

    bool Execute(Event event) override;
};

#endif
