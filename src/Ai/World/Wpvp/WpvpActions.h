#ifndef PLAYERBOTS_WPVPACTIONS_H
#define PLAYERBOTS_WPVPACTIONS_H

#include "Action.h"
#include "AttackAction.h"

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

// The one bored-raid dice roll of this excursion, and - when it passes - the
// raid itself: attack the nearest clearly-outleveled hostile guard (or, knob
// permitting, flight master) near the hub. The guard's death fires the
// faction-wide zone-under-attack alarm, which is the point: defenders come,
// and the bored invader gets its fight.
class WpvpRaidAction : public AttackAction
{
public:
    WpvpRaidAction(PlayerbotAI* botAI) : AttackAction(botAI, "wpvp raid") {}

    bool Execute(Event event) override;
};

#endif
