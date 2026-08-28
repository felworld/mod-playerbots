/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "CombatDirective.h"

#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"
#include "Timer.h"

namespace
{
// Live bots clear their own dead orders on their next tick, so the only thing
// left to sweep is bots that logged out while holding one.
constexpr uint32 PRUNE_INTERVAL_MS = 5 * MINUTE * IN_MILLISECONDS;

// Orders come in opposed pairs, and a new one replaces the pair's previous
// order rather than stacking with it.
enum DirectiveTopic : uint32
{
    TOPIC_AOE = 0,
    TOPIC_MANA = 1,
};

uint32 TopicOf(CombatDirective directive)
{
    switch (directive)
    {
        case CombatDirective::NoAoe:
        case CombatDirective::AoeOk:
            return TOPIC_AOE;
        default:
            return TOPIC_MANA;
    }
}

// One combat strategy the order wants present, or gone.
struct StrategyOp
{
    char const* name;
    bool present;
};

// What each order does to a bot's combat strategies.
//
// AoE has no single strategy name: most classes file theirs as "aoe", priests
// as "shadow aoe", death knights split it by spec, and paladins have none at
// all. So the order names every candidate and only the ones this bot actually
// runs are touched.
std::vector<StrategyOp> OpsFor(CombatDirective directive)
{
    switch (directive)
    {
        case CombatDirective::NoAoe:
            return {{"aoe", false}, {"shadow aoe", false}, {"frost aoe", false}, {"unholy aoe", false}};
        case CombatDirective::ConserveMana:
            return {{"save mana", true}};
        case CombatDirective::ManaFree:
            return {{"save mana", false}};
        default:
            // AoeOk is a pure countermand. There is no class-independent way
            // to switch AoE back on - blindly adding every candidate would
            // hand a frost death knight the unholy rotation - so lifting the
            // order means undoing exactly what it removed, nothing more.
            return {};
    }
}

// Run strategy tokens ("+aoe", "-save mana") through the same entry point the
// `co` command uses.
void RunTokens(PlayerbotAI* botAI, std::vector<std::string> const& tokens)
{
    for (auto const& token : tokens)
        botAI->ChangeStrategy(token, BOT_STATE_COMBAT);
}

// Apply an order's ops, returning the tokens that undo them. A strategy the
// bot's class does not know silently stays as it was, and is not recorded -
// there is nothing to give back.
std::vector<std::string> ApplyOps(PlayerbotAI* botAI, std::vector<StrategyOp> const& ops)
{
    std::vector<std::string> revert;
    for (auto const& op : ops)
    {
        std::string const name(op.name);
        if (botAI->HasStrategy(name, BOT_STATE_COMBAT) == op.present)
            continue;

        botAI->ChangeStrategy((op.present ? "+" : "-") + name, BOT_STATE_COMBAT);
        if (botAI->HasStrategy(name, BOT_STATE_COMBAT) != op.present)
            continue;

        revert.push_back((op.present ? "-" : "+") + name);
    }
    return revert;
}

bool StillBinding(Player* bot, ObjectGuid issuer)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(issuer);
    if (!player)
        return false;

    Group* group = bot->GetGroup();
    return group && group == player->GetGroup();
}
}  // namespace

void CombatDirectiveBoard::OrderGroup(Player* issuer, CombatDirective directive)
{
    if (!issuer)
        return;

    Group* group = issuer->GetGroup();
    if (!group)
        return;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != issuer && GET_PLAYERBOT_AI(member))
            OrderBot(member, issuer, directive);
    }
}

void CombatDirectiveBoard::OrderBot(Player* bot, Player* issuer, CombatDirective directive)
{
    if (!bot || !issuer || directive == CombatDirective::None)
        return;

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(getMSTime());

    uint32 const topic = TopicOf(directive);
    std::vector<Entry>& entries = _orders[bot->GetGUID()];
    for (Entry& entry : entries)
    {
        if (TopicOf(entry.directive) != topic)
            continue;

        // The previous order on this topic is over, but its strategy changes
        // are still on the bot: carry the undo tokens into the new order so
        // the bot's own tick puts things back before taking this one up.
        entry.issuer = issuer->GetGUID();
        entry.directive = directive;
        entry.applied = false;
        _active.store(true, std::memory_order_relaxed);
        return;
    }

    entries.push_back(Entry{issuer->GetGUID(), directive, false, {}});
    _active.store(true, std::memory_order_relaxed);
}

void CombatDirectiveBoard::Sync(Player* bot, PlayerbotAI* botAI)
{
    if (!bot || !botAI || !_active.load(std::memory_order_relaxed))
        return;

    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _orders.find(bot->GetGUID());
    if (it == _orders.end())
        return;

    // Strategy work happens under the lock. It only runs on the tick an order
    // actually changes, and only ever touches this bot.
    std::vector<Entry>& entries = it->second;
    for (auto entry = entries.begin(); entry != entries.end();)
    {
        if (!StillBinding(bot, entry->issuer))
        {
            RunTokens(botAI, entry->revert);
            entry = entries.erase(entry);
            continue;
        }

        if (entry->applied)
        {
            ++entry;
            continue;
        }

        RunTokens(botAI, entry->revert);
        entry->revert = ApplyOps(botAI, OpsFor(entry->directive));
        entry->applied = true;

        // A countermand changes nothing on its own - once it has given back
        // what the order it lifted took, there is no standing order left.
        if (entry->revert.empty() && OpsFor(entry->directive).empty())
            entry = entries.erase(entry);
        else
            ++entry;
    }

    if (entries.empty())
        _orders.erase(it);
    if (_orders.empty())
        _active.store(false, std::memory_order_relaxed);
}

std::string CombatDirectiveBoard::Describe(Player* bot)
{
    if (!bot || !_active.load(std::memory_order_relaxed))
        return "";

    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _orders.find(bot->GetGUID());
    if (it == _orders.end())
        return "";

    std::string described;
    for (Entry const& entry : it->second)
    {
        Player* issuer = ObjectAccessor::FindConnectedPlayer(entry.issuer);
        if (!issuer || !StillBinding(bot, entry.issuer))
            continue;

        if (!described.empty())
            described += ", ";

        described += DirectiveName(entry.directive);
        described += " (ordered by ";
        described += issuer->GetName();
        described += ")";
    }
    return described;
}

char const* CombatDirectiveBoard::DirectiveName(CombatDirective directive)
{
    switch (directive)
    {
        case CombatDirective::NoAoe:
            return "no AoE";
        case CombatDirective::AoeOk:
            return "AoE allowed";
        case CombatDirective::ConserveMana:
            return "rationing mana";
        case CombatDirective::ManaFree:
            return "spending mana freely";
        default:
            return "";
    }
}

CombatDirective CombatDirectiveBoard::FromName(std::string const& name)
{
    if (name == "no_aoe")
        return CombatDirective::NoAoe;
    if (name == "aoe_ok")
        return CombatDirective::AoeOk;
    if (name == "conserve_mana")
        return CombatDirective::ConserveMana;
    if (name == "mana_free")
        return CombatDirective::ManaFree;

    return CombatDirective::None;
}

void CombatDirectiveBoard::Prune(uint32 now)
{
    if (getMSTimeDiff(_lastPruneMs, now) < PRUNE_INTERVAL_MS)
        return;

    _lastPruneMs = now;
    // A bot that logged out took its strategies with it - nothing was ever
    // persisted, so there is nothing to hand back, only the entry to drop.
    std::erase_if(_orders, [](auto const& pair) { return !ObjectAccessor::FindConnectedPlayer(pair.first); });
}
