#ifndef PLAYERBOTS_WPVPCHASE_H
#define PLAYERBOTS_WPVPCHASE_H

#include <mutex>
#include <unordered_map>

#include "Define.h"
#include "ObjectGuid.h"

class Player;
class Unit;

// Chase leash (Felworld): nothing in the pursuit path had a distance or time
// bound, so a bot would chase a fleeing world-PvP target across the continent.
// Instead of a hard leash, a bot whose target has broken contact (no damage
// either way and beyond close range) rolls AiPlayerbot.WpvpChaseBreakChance to
// abandon the chase after each WpvpChaseBreakSeconds{Min,Max} interval - a
// geometric falloff, so most bots give up within a roll or two while an
// occasional one stays dogged.

// Drive from the pursuit path (the current-target validity check): updates the
// contact clock, runs due break rolls, and returns true once the chase is
// broken - which makes the target invalid and drops it.
bool WpvpChaseBroken(Player* bot, Unit* target);

// Passive gate for the (re)acquisition paths: true while the bot has an
// abandoned chase standing against this player. The ban is behavioral, not
// timed - it clears when the runner closes back in on the bot (a re-entrance
// invites a re-chase) or lands a hit (self-defense), or once they've been out
// of the bot's life long enough for the record to expire.
bool WpvpChaseBanned(Player* bot, Unit* target);

// Directional (chaser -> runner) pursuit and abandoned-chase records plus a
// symmetric last-damage clock per pair. All state is mutex-guarded - the
// damage hook and target-evaluation loops run on different map-update
// threads.
class WpvpChaseBoard
{
public:
    static WpvpChaseBoard& instance()
    {
        static WpvpChaseBoard instance;
        return instance;
    }

    // From the unit damage hook: refreshes the contact clock for an opposing
    // player pair and re-arms any abandoned chase between them.
    void NoteDamage(Unit* attacker, Unit* victim);

    bool UpdatePursuit(Player* bot, Player* target);
    bool IsBanned(Player* bot, Player* target);

private:
    WpvpChaseBoard() = default;

    struct Pursuit
    {
        uint32 nextRollMs;  // 0 while in contact
        uint32 touchedMs;
    };

    struct Ban
    {
        float breakDistance;
        uint32 touchedMs;
        uint64 targetRaw;  // the runner, so a swing of theirs can clear by scan
    };

    void Prune(uint32 now);

    std::mutex _mutex;
    std::unordered_map<uint64, uint32> _lastDamageMs;  // symmetric pair
    std::unordered_map<uint64, Pursuit> _pursuits;     // chaser -> runner
    std::unordered_map<uint64, Ban> _bans;             // chaser -> runner
};

#endif
