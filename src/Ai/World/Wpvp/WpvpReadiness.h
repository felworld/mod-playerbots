#ifndef PLAYERBOTS_WPVPREADINESS_H
#define PLAYERBOTS_WPVPREADINESS_H

class Player;

// Initiation readiness (Felworld): before picking an unprovoked fight a
// player glances at their own bars and asks "do I have more advantage now,
// or after I'm full up?". A bot below the WpvpInitiateSelfHealth/Mana
// comfort bars declines to start a fight it could have waited out - unless
// "now" is clearly better: the target is already trading blows with other
// players, reads well below the bot's level, or is the bot's revenge
// grudge. Self-defense and party assists never come through here, and
// battlegrounds are exempt (everyone there already came to fight).

// The bot's own bars alone: above both comfort thresholds.
bool WpvpBarsReadyToInitiate(Player* bot);

// The full ledger: bars, or a target-state advantage that beats waiting.
bool WpvpReadyToInitiate(Player* bot, Player* target);

// Called when the readiness gate declines a target: remembers the pass so
// the "wpvp drink up" trigger can sit the bot down to fix its bars while
// the opportunity is still warm (and logs the stand-down, throttled).
void WpvpNoteGatedOpportunity(Player* bot, Player* target);

// A gated opportunity was noted within the drink-up window.
bool WpvpGatedOpportunityRecent(Player* bot);

#endif
