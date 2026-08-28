/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_COMBATDIRECTIVE_H
#define PLAYERBOTS_COMBATDIRECTIVE_H

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ObjectGuid.h"

class Player;
class PlayerbotAI;

// A standing combat order one player gave the bots around them - "no AoE on
// this pull", "go easy on mana". Each one names a pair of opposites so an
// order can be lifted as plainly as it was given.
enum class CombatDirective : uint32
{
    None = 0,
    NoAoe,         // stop using area-of-effect abilities
    AoeOk,         // area-of-effect is fine again: lifts an earlier NoAoe
    ConserveMana,  // ration mana
    ManaFree,      // spend mana freely
};

// Orders that outlive the moment they were given.
//
// The `co` command already toggles a bot's strategies, and real players keep
// driving it directly - but a raw `co -aoe` is one bot's setting, permanent
// until somebody countermands it, still in force long after the party that
// asked for it disbanded. An order given to a party should die with it: a bot
// told "no AoE" in a dungeon must not spend the rest of the night refusing to
// AoE alone in Un'Goro.
//
// So the order is recorded here rather than burned into the bot, and the bot
// reconciles itself against the board: it applies an order it has not taken
// up yet, and undoes exactly what it applied once the issuer is no longer in
// its group. Nothing is persisted - the saved strategy list stays whatever
// `co`/`nc` last wrote.
//
// Written from the world thread (mod-llm's combat_directive tool) and read
// from map-update threads (each bot's own tick), so the board is mutex-guarded
// like GroupChatterBoard.
class CombatDirectiveBoard
{
public:
    static CombatDirectiveBoard& instance()
    {
        static CombatDirectiveBoard instance;
        return instance;
    }

    // Order every playerbot grouped with `issuer` to follow `directive`. A
    // directive is an order, not a suggestion: everyone in earshot takes it
    // up, and only the bot that was spoken to answers for the party.
    void OrderGroup(Player* issuer, CombatDirective directive);

    // Order one bot, for an instruction addressed to it alone.
    void OrderBot(Player* bot, Player* issuer, CombatDirective directive);

    // Reconcile `bot` against its standing orders: take up what is new, undo
    // what the ended party left behind. Called from the bot's own update tick
    // so strategy changes always happen on the thread that owns the bot.
    void Sync(Player* bot, PlayerbotAI* botAI);

    // The orders `bot` is holding to, phrased for a prompt - "no AoE (ordered
    // by Ledeyn)" - or empty when it is under none.
    std::string Describe(Player* bot);

    // Prompt-facing name of a directive, e.g. "no AoE".
    static char const* DirectiveName(CombatDirective directive);

    // Parse a tool/command vocabulary word ("no_aoe", ...); None when unknown.
    static CombatDirective FromName(std::string const& name);

private:
    CombatDirectiveBoard() = default;

    struct Entry
    {
        ObjectGuid issuer;
        CombatDirective directive{CombatDirective::None};
        // The order's own strategy changes are done: cleared again when a
        // fresh order lands on the same topic.
        bool applied{false};
        // Strategy tokens ("+aoe", "-save mana") that undo what this bot
        // currently has applied for this topic, in the form
        // PlayerbotAI::ChangeStrategy takes.
        std::vector<std::string> revert;
    };

    void Prune(uint32 now);

    std::mutex _mutex;
    // Per bot, at most one standing order per topic (AoE, mana).
    std::unordered_map<ObjectGuid, std::vector<Entry>> _orders;
    // Lets every bot's tick skip the lock entirely while nobody has given an
    // order, which is the overwhelmingly common case.
    std::atomic<bool> _active{false};
    uint32 _lastPruneMs{0};
};

#endif
