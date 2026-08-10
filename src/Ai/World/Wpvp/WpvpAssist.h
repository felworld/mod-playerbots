#ifndef PLAYERBOTS_WPVPASSIST_H
#define PLAYERBOTS_WPVPASSIST_H

class Player;

// Passerby assist and kill-the-add targeting (Felworld): on a PvP world it is
// rare for anyone to ignore a fight in front of them - etiquette demands
// jumping in - and conversely an underleveled helper who joins one is
// commonly focused down first, like an add in a dungeon.

// The candidate is actively fighting players: a living player sits in their
// PvP combat refs (duels excluded - a duel is a private fight, not a battle).
bool WpvpActivePvpCombatant(Player* candidate);

// Target-selection score for open-world PvP: lower is more attractive. Base
// is raw distance; a candidate the bot perceivedly outlevels reads as an
// easier kill from further away, and one already trading blows with players
// (the add who just joined a brawl, or any active combatant) pulls harder
// still. EnemyPlayerValue's sighting sort, its peel margin and
// WpvpPeelTrigger all compare this same number, so target selection and
// peeling can never disagree.
float WpvpTargetScore(Player* bot, Player* candidate);

// Passerby attack-assist: this enemy is fair game past the usual courage
// gates because they are attacking a faction-mate right in front of the bot.
// Holds when the bot is a solo, already-flagged world passerby (etiquette
// never demands flagging yourself), the enemy is trading blows with a living
// faction-mate inside AiPlayerbot.WpvpPasserbyAssistRadius of the bot, the
// enemy's frame shows an actual level (a "??" skull means massacre, not
// fight - support heals are the bystander-assist path's job), and the
// deterministic WpvpPasserbyAssistChance dice pass for this bot/enemy pair.
bool WpvpPasserbyAssistTarget(Player* bot, Player* enemy);

#endif
