#ifndef PLAYERBOTS_WPVPGRUDGE_H
#define PLAYERBOTS_WPVPGRUDGE_H

#include <mutex>
#include <unordered_map>
#include <vector>

#include "MovementActions.h"
#include "ObjectGuid.h"
#include "Trigger.h"

class Player;
class PlayerbotAI;

// Victim-side memory of a world-PvP death (Felworld): a bot that gets killed
// remembers its killer for AiPlayerbot.WpvpGrudgeMinutes instead of rezzing
// back into serene quest-grinding while they stand there. On death it rolls
// AiPlayerbot.WpvpRevengeChance to want revenge - the killer becomes an
// attack-on-sight priority target, past the usual courage dice and truces.
// A bot that fails the roll (or whose killer plainly outclasses it, or that
// dies to the same killer twice) turns avoidant instead: it never initiates
// against them, and gets out of the way when they come near - pleading
// (/shoo, /beg, /cry) and retreating. A successful revenge settles the
// grudge; otherwise it expires on its own.
enum class WpvpGrudgeDisposition : uint8
{
    None,
    Revenge,
    Avoidant,
};

// Revenge wants a winnable rematch: an enemy who reads this far above the
// bot inspires fear instead (mirrors the courage gates' EXTREME_LEVEL_DIFF -
// revenge bypasses those dice, so the suicide line is enforced here).
constexpr int32 WPVP_REVENGE_OUTCLASS_GAP = 5;

// The bot's standing toward this enemy: the short-term reflex grudge above
// when one stands, else the persistent vendetta ledger (WpvpVendetta), else
// None.
WpvpGrudgeDisposition WpvpGrudgeAgainst(Player* bot, Player* enemy);

// The nearest living enemy within range that the bot avoids (reflex grudge
// or vendetta), can see, and is not already trading blows with (once they
// attack, self-defense owns the bot and dodging them stops making sense).
Player* WpvpAvoidantKillerNear(Player* bot, float range);

// From the text-emote hook: a /beg, /cry, or /shoo by someone under attack
// (or targeted at a bot stalking them) rolls AiPlayerbot.WpvpBegMercyChance
// per bot attacker to move them to mercy - they break off and leave the
// beggar alone via a chase-board ban, which the beggar's own next swing
// clears. The roll is deterministic over a short window, so emote spam
// doesn't reroll it.
void NoteMercyPlea(Player* emoter, uint32 textEmote, ObjectGuid targetGuid);

// Directional (victim -> killer) grudge records. All state is mutex-guarded -
// the PvP-kill hook and target-evaluation loops run on different map-update
// threads.
class WpvpGrudgeBoard
{
public:
    static WpvpGrudgeBoard& instance()
    {
        static WpvpGrudgeBoard instance;
        return instance;
    }

    // From the PVP-kill hook (world kills only): settles the killer's own
    // grudge against this victim if one stands (revenge achieved), then - if
    // the victim is a bot - records the victim's grudge and rolls its
    // disposition.
    void RecordKill(Player* killer, Player* victim);

    WpvpGrudgeDisposition Disposition(ObjectGuid victim, ObjectGuid killer);

    // Guids of every unexpired avoidant grudge this victim holds.
    std::vector<ObjectGuid> AvoidantKillers(ObjectGuid victim);

    // Claim the right to deliver one plea emote at this killer; false while
    // the per-grudge cooldown is still running (or no avoidant grudge
    // stands).
    bool ClaimPleaEmote(ObjectGuid victim, ObjectGuid killer);

private:
    WpvpGrudgeBoard() = default;

    struct Grudge
    {
        WpvpGrudgeDisposition disposition{WpvpGrudgeDisposition::None};
        uint32 deaths{0};
        uint32 untilMs{0};
        uint32 nextPleaMs{0};
    };

    void Prune(uint32 now);

    std::mutex _mutex;
    std::unordered_map<ObjectGuid, std::unordered_map<ObjectGuid, Grudge>> _grudges;  // victim -> killer
};

// An avoidant-grudge killer is close: get out of their way.
class WpvpAvoidKillerTrigger : public Trigger
{
public:
    WpvpAvoidKillerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp avoid killer", 2) {}

    bool IsActive() override;
};

// Face the killer, deliver a plea emote (throttled), and retreat out of
// their reach - repeatedly, so a killer who keeps closing keeps being fled.
class WpvpAvoidKillerAction : public MovementAction
{
public:
    WpvpAvoidKillerAction(PlayerbotAI* botAI) : MovementAction(botAI, "wpvp avoid killer") {}

    bool Execute(Event event) override;
};

#endif
