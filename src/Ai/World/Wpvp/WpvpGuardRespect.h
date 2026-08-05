#ifndef PLAYERBOTS_WPVPGUARDRESPECT_H
#define PLAYERBOTS_WPVPGUARDRESPECT_H

class Player;
class Unit;

// Guard respect (Felworld): true when open-world PvP pursuit of this player
// should be declined - or an ongoing chase broken off - because the fight sits
// under the cover of hostile guards that outlevel the bot by
// AiPlayerbot.WpvpGuardRespectLevelGap or more (0 disables). A human doesn't
// chase a fleeing enemy past guards that would flatten them; without this bar
// bots did exactly that and died to town guards way above their level.
// Guards near the TARGET refuse the fight before it starts (the enemy is in
// their guards' protective bubble); guards near the BOT break a chase off
// just before it crosses the guard line. Instanced PvP and the bot's duel
// opponent are exempt - there are no world guards to respect there.
bool WpvpGuardsBarPursuit(Player* bot, Unit* target);

#endif
