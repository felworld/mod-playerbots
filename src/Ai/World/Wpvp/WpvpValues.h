#ifndef PLAYERBOTS_WPVPVALUES_H
#define PLAYERBOTS_WPVPVALUES_H

#include "PossibleTargetsValue.h"

// Nearby opposing-faction players who are NOT PvP-flagged - the goad targets
// for excursion bots. Same grid-scan family as NearestEnemyPlayersValue, but
// with its own acceptance filter (the shared one rejects unflagged players
// outright).
class NearestUnflaggedEnemyPlayersValue : public PossibleTargetsValue
{
public:
    NearestUnflaggedEnemyPlayersValue(PlayerbotAI* botAI, float range = sPlayerbotAIConfig.wpvpVisionDistance)
        : PossibleTargetsValue(botAI, "nearest unflagged enemy players", range)
    {
    }

protected:
    bool AcceptUnit(Unit* unit) override;
};

#endif
