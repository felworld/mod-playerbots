#ifndef PLAYERBOTS_WPVPTRUCE_H
#define PLAYERBOTS_WPVPTRUCE_H

#include <mutex>
#include <unordered_map>

#include "Action.h"
#include "ObjectGuid.h"
#include "Trigger.h"

class Player;
class PlayerbotAI;

// The old same-class courtesy ("druids don't gank druids"): true when this
// bot spares that same-class enemy instead of attacking or goading them.
// Deterministic per character pair - whether the pair falls under the code
// is a symmetric permanent trait, so the decision can't flip mid-standoff;
// on top of that a small directional oathbreaker roll means one side may
// honor the truce while the other takes the salute and swings anyway.
// Per-class chances come from AiPlayerbot.WpvpClassTruceChance; a known
// ganker (on the defense board) forfeits the courtesy, and instanced PvP
// never observes it.
bool WpvpTruceHolds(Player* bot, Player* enemy);

// Bookkeeping for the salute that replaces the attack. The truce gates run
// inside target-evaluation loops, so they only queue the gesture here; the
// "wpvp truce salute" trigger/action pair delivers it once the bot is free.
// All state is mutex-guarded (bot AI runs on map-update workers).
class WpvpTruceBoard
{
public:
    static WpvpTruceBoard& instance()
    {
        static WpvpTruceBoard instance;
        return instance;
    }

    // The bot just declined this enemy under the truce: queue a salute if
    // they're close enough for the gesture to be seen and the pair hasn't
    // exchanged formalities recently.
    void NotePassing(Player* bot, Player* target);

    // Cheap peek for the trigger: a still-fresh queued salute exists.
    bool HasPending(ObjectGuid bot);

    // Pop the queued salute and record the pair as saluted (one attempt per
    // pair per cooldown, delivered or not). Empty when nothing fresh.
    ObjectGuid ClaimPending(ObjectGuid bot);

private:
    WpvpTruceBoard() = default;

    void Prune(uint32 now);

    struct PendingSalute
    {
        ObjectGuid target;
        uint32 queuedMs{0};
    };

    std::mutex _mutex;
    std::unordered_map<ObjectGuid, PendingSalute> _pending;
    std::unordered_map<uint64, uint32> _saluted;
};

// A salute queued by one of the truce gates is waiting and the bot is free
// to deliver it.
class WpvpTruceSaluteTrigger : public Trigger
{
public:
    WpvpTruceSaluteTrigger(PlayerbotAI* botAI) : Trigger(botAI, "wpvp truce salute", 2) {}

    bool IsActive() override;
};

// Face the spared enemy and deliver a targeted /salute - stealthers step out
// first; the reveal is part of the gesture.
class WpvpTruceSaluteAction : public Action
{
public:
    WpvpTruceSaluteAction(PlayerbotAI* botAI) : Action(botAI, "wpvp truce salute") {}

    bool Execute(Event event) override;
};

#endif
