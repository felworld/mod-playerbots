#ifndef PLAYERBOTS_WPVPVALUES_H
#define PLAYERBOTS_WPVPVALUES_H

#include "PossibleTargetsValue.h"

// Nearby opposing-faction players who are NOT PvP-flagged - the goad targets
// for stealthed excursion bots. Same grid-scan family as
// NearestEnemyPlayersValue, and keeps the parent's level-difference filter:
// don't goad someone the bot wouldn't actually fight.
class NearestUnflaggedEnemyPlayersValue : public PossibleTargetsValue
{
public:
    NearestUnflaggedEnemyPlayersValue(PlayerbotAI* botAI, float range = sPlayerbotAIConfig.grindDistance)
        : PossibleTargetsValue(botAI, "nearest unflagged enemy players", range)
    {
    }

protected:
    bool AcceptUnit(Unit* unit) override;
};

#endif
