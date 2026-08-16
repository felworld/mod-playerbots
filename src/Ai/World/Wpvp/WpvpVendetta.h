#ifndef PLAYERBOTS_WPVPVENDETTA_H
#define PLAYERBOTS_WPVPVENDETTA_H

#include <mutex>
#include <unordered_map>
#include <vector>

#include "ObjectGuid.h"
#include "WpvpGrudge.h"

class Player;
class Unit;

// The persistent layer under the grudge board (Felworld): every unprovoked
// world-PvP death a bot suffers is tallied here per killer, in the
// playerbots database, surviving restarts - and enough of them harden into
// a vendetta that never expires on its own. Deaths in fights the bot itself
// picked (a revenge sortie, a courage-dice initiation, riding out to a
// defense call) never count: losing a fight you chose breeds no resentment,
// being hunted does. Re-kills inside the camping window reach the threshold
// quicker. An open vendetta reads as a standing revenge grudge - attack on
// sight, past courage dice, satiation and truces - or, while the killer
// still plainly outclasses the bot, as fear: avoid and plead, turning to
// vengeance once the bot catches up in levels. Killing the offender settles
// the vendetta - and so does a successful escape: when the last encounter
// ended with the bot fleeing and pleading and the tormentor let it live,
// the vendetta is forgiven at the next sighting. Either way the tally
// stays on the books, and one fresh gank re-opens it. Real players get
// ledger entries as killers only - a human's resentment is their own.
class WpvpVendettaBoard
{
public:
    static WpvpVendettaBoard& instance()
    {
        static WpvpVendettaBoard instance;
        return instance;
    }

    // Synchronous read of the whole ledger at server startup, before any
    // bot logs in.
    void LoadFromDB();

    // The bot chose to start this fight (it wasn't already trading blows
    // with the enemy), so a death against this enemy in the near term is
    // its own doing. The mark is kept fresh by RefreshInitiated while the
    // bot keeps landing blows, and dies with the bot.
    void NoteBotInitiated(Player* bot, Player* enemy);
    void RefreshInitiated(Unit* attacker, Unit* victim);

    // From the PVP-kill hook (world kills only): a kill settles any open
    // vendetta the killer held against this victim, and - if the victim is
    // a bot that didn't pick this fight - tallies the gank, re-opening a
    // settled vendetta.
    void RecordKill(Player* killer, Player* victim);

    // The bot is fleeing this enemy (WpvpAvoidKillerAction), whichever
    // board drove the retreat. If the encounter ends without a re-kill,
    // the open vendetta is forgiven at the next sighting - the pleas
    // worked, the griefer stopped.
    void NoteFled(Player* bot, Player* enemy);

    // The vendetta's contribution to WpvpGrudgeAgainst: None without an
    // open vendetta; Avoidant while the enemy reads WPVP_REVENGE_OUTCLASS_GAP
    // levels above the bot; Revenge otherwise. Settles a vendetta whose
    // last encounter was a successful escape (see NoteFled).
    WpvpGrudgeDisposition Disposition(Player* bot, Player* enemy);

    // Guids of every enemy this bot holds an open vendetta against.
    std::vector<ObjectGuid> VendettaEnemies(ObjectGuid bot);

    // Claim the right to deliver one plea emote at this enemy; false while
    // the cooldown is still running (or no open vendetta stands).
    bool ClaimPleaEmote(ObjectGuid bot, ObjectGuid enemy);

private:
    WpvpVendettaBoard() = default;

    struct Vendetta
    {
        uint32 ganks{0};
        uint32 camps{0};
        uint32 lastGankAt{0};  // epoch seconds
        bool settled{false};
        uint32 lastFledAt{0};  // epoch seconds, in-memory only
        uint32 nextPleaMs{0};  // in-memory only
    };

    bool Open(Vendetta const& vendetta) const;
    void Persist(ObjectGuid victim, ObjectGuid killer, Vendetta const& vendetta);

    std::mutex _mutex;
    std::unordered_map<ObjectGuid, std::unordered_map<ObjectGuid, Vendetta>> _ledger;     // victim -> killer
    std::unordered_map<ObjectGuid, std::unordered_map<ObjectGuid, uint32>> _initiated;    // bot -> enemy -> untilMs
};

#endif
