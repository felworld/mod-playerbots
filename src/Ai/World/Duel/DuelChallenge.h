#ifndef PLAYERBOTS_DUELCHALLENGE_H
#define PLAYERBOTS_DUELCHALLENGE_H

#include "MovementActions.h"
#include "SharedDefines.h"
#include "Trigger.h"

class Player;
class PlayerbotAI;

// A bot with the "start duel" strategy is free (idle enough, healthy, no
// hostiles, duels allowed here, challenge cooldown up) and a challengeable
// player - bot or real - is nearby. Cooldowns are much shorter while
// dwelling at a gate duel spot than while roaming.
class StartDuelPossibleTrigger : public Trigger
{
public:
    StartDuelPossibleTrigger(PlayerbotAI* botAI) : Trigger(botAI, "start duel possible", 5) {}

    bool IsActive() override;

    // Shared with StartDuelAction so the trigger and the action agree on the
    // target.
    static Player* FindDuelTarget(PlayerbotAI* botAI, Player* bot);
};

// Walk up to the chosen target and cast the duel challenge (spell 7266).
class StartDuelAction : public MovementAction
{
public:
    StartDuelAction(PlayerbotAI* botAI) : MovementAction(botAI, "start duel") {}

    bool Execute(Event event) override;
};

// Post-duel flavor for bots, called from the OnPlayerDuelEnd hook: winner
// cheers (sometimes with a line), loser cries/salutes.
void OnBotDuelEnded(Player* winner, Player* loser, DuelCompleteType type);

#endif
