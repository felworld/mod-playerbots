#ifndef PLAYERBOTS_NEWRPGDUELSPOT_H
#define PLAYERBOTS_NEWRPGDUELSPOT_H

#include "NewRpgBaseAction.h"

// Fill in hub/anchor/teleport positions for a duel-spot hangout at the bot's
// faction capital gates (Stormwind for Alliance, Orgrimmar for Horde).
// Returns false if the hub's map isn't loaded.
bool ComputeDuelSpotPositions(Player* bot, NewRpgInfo::DuelSpot& out);

// The hub location a bot of this team hangs out at, so availability checks
// can reason about it without building the full payload.
WorldLocation const& GetDuelSpotHub(TeamId team);

// Shared teardown for a duel-spot hangout: removes the "start duel" strategy
// if the hangout added it and returns the bot to idle.
void EndDuelSpotHangout(PlayerbotAI* botAI, char const* reason);

class NewRpgDuelSpotAction : public NewRpgBaseAction
{
public:
    NewRpgDuelSpotAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg duel spot") {}
    bool Execute(Event event) override;

private:
    bool GuardedTeleport(NewRpgInfo::DuelSpot& data);
    bool Dwell(NewRpgInfo::DuelSpot& data);
    bool Solicit(NewRpgInfo::DuelSpot& data);
};

#endif
