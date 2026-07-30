#include "WpvpTruce.h"

#include <algorithm>

#include "EmoteAction.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "WpvpDefense.h"

namespace
{
// The gesture only reads if the other side actually sees it: queue the salute
// only when the paths cross inside comfortable text-emote earshot.
constexpr float TRUCE_SALUTE_RANGE = 30.0f;

// A queued salute is a "we just crossed paths" beat - if the bot can't
// deliver it promptly (mid-combat, target gone), let it lapse.
constexpr uint32 PENDING_TTL_MS = 30 * IN_MILLISECONDS;

// Crossing paths with the same spared enemy again soon doesn't warrant
// another round of formalities.
constexpr uint32 SALUTE_COOLDOWN_MS = 10 * MINUTE * IN_MILLISECONDS;

// 64 bit FNV-1a hash constants, same scheme as PossibleTargetsValue's
// level-gap dice.
constexpr uint64 FNV_OFFSET_BASIS = 14695981039346656037ULL;
constexpr uint64 FNV_PRIME = 1099511628211ULL;

// Salt decorrelating the directional oathbreaker roll from the pair roll.
constexpr uint64 OATHBREAKER_SALT = 0x4F41544842524B52ULL;

uint64 HashPair(uint64 first, uint64 second)
{
    uint64 hash = FNV_OFFSET_BASIS;
    hash ^= first;
    hash *= FNV_PRIME;
    hash ^= second;
    hash *= FNV_PRIME;
    return hash;
}
}

bool WpvpTruceHolds(Player* bot, Player* enemy)
{
    uint8 const botClass = bot->getClass();
    if (botClass != enemy->getClass())
        return false;

    uint32 const chance = sPlayerbotAIConfig.wpvpClassTruceChance[botClass];
    if (!chance)
        return false;

    // A world courtesy only - instanced PvP is there to be fought.
    if (bot->InBattleground() || bot->InArena())
        return false;

    // A ganker the defense channels already named forfeits the courtesy.
    if (WpvpDefenseBoard::instance().IsKnownThreat(enemy->GetGUID(), bot->GetTeamId()))
        return false;

    // Unordered pair, no time component: whether the pair falls under the
    // code is a permanent trait of the pair, seen the same from both sides.
    uint64 lo = bot->GetGUID().GetRawValue();
    uint64 hi = enemy->GetGUID().GetRawValue();
    if (lo > hi)
        std::swap(lo, hi);

    if ((HashPair(lo, hi) % 100) >= chance)
        return false;

    // ...but honoring it is personal: a directional roll makes a share of
    // individuals oathbreakers toward this rival, so one side may salute
    // and the other swing anyway. Equally permanent, equally deterministic.
    uint64 const dirHash =
        HashPair(bot->GetGUID().GetRawValue() ^ OATHBREAKER_SALT, enemy->GetGUID().GetRawValue());
    return (dirHash % 100) >= sPlayerbotAIConfig.wpvpTruceOathbreakerChance;
}

void WpvpTruceBoard::NotePassing(Player* bot, Player* target)
{
    if (!bot->IsWithinDist(target, TRUCE_SALUTE_RANGE))
        return;

    uint32 now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(now);

    auto saluted = _saluted.find(HashPair(bot->GetGUID().GetRawValue(), target->GetGUID().GetRawValue()));
    if (saluted != _saluted.end())
        return;

    // Keep the original queue time on repeat sightings so a bot stuck in
    // combat doesn't hold a forever-fresh salute; the gate re-queues later if
    // the two are still near each other.
    PendingSalute& pending = _pending[bot->GetGUID()];
    if (pending.target != target->GetGUID())
    {
        pending.target = target->GetGUID();
        pending.queuedMs = now;
    }
}

bool WpvpTruceBoard::HasPending(ObjectGuid bot)
{
    uint32 now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _pending.find(bot);
    return it != _pending.end() && now - it->second.queuedMs <= PENDING_TTL_MS;
}

ObjectGuid WpvpTruceBoard::ClaimPending(ObjectGuid bot)
{
    uint32 now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _pending.find(bot);
    if (it == _pending.end())
        return ObjectGuid::Empty;

    ObjectGuid const target = it->second.target;
    bool const fresh = now - it->second.queuedMs <= PENDING_TTL_MS;
    _pending.erase(it);

    if (!fresh)
        return ObjectGuid::Empty;

    _saluted[HashPair(bot.GetRawValue(), target.GetRawValue())] = now;
    return target;
}

void WpvpTruceBoard::Prune(uint32 now)
{
    for (auto it = _pending.begin(); it != _pending.end();)
    {
        if (now - it->second.queuedMs > PENDING_TTL_MS)
            it = _pending.erase(it);
        else
            ++it;
    }

    for (auto it = _saluted.begin(); it != _saluted.end();)
    {
        if (now - it->second > SALUTE_COOLDOWN_MS)
            it = _saluted.erase(it);
        else
            ++it;
    }
}

bool WpvpTruceSaluteTrigger::IsActive()
{
    if (!bot->IsAlive() || bot->IsInCombat())
        return false;

    return WpvpTruceBoard::instance().HasPending(bot->GetGUID());
}

bool WpvpTruceSaluteAction::Execute(Event /*event*/)
{
    ObjectGuid const targetGuid = WpvpTruceBoard::instance().ClaimPending(bot->GetGUID());
    if (!targetGuid)
        return false;

    Unit* target = botAI->GetUnit(targetGuid);
    if (!target || !target->IsAlive() || !bot->IsWithinDist(target, TRUCE_SALUTE_RANGE + 10.0f) ||
        !bot->IsWithinLOSInMap(target))
        return false;

    // Stealthers step out to deliver it - the reveal is part of the gesture.
    botAI->RemoveAura("stealth");
    botAI->RemoveAura("prowl");
    bot->SetFacingToObject(target);

    // Targeted text emote so "<name> salutes you" lands in the other side's
    // chat log. The emote-alert board exempts sincere respect gestures, so
    // this doesn't rally the bot's own faction onto the salutee.
    ObjectGuid const oldSelection = bot->GetTarget();
    bot->SetSelection(targetGuid);

    WorldPacket data(SMSG_TEXT_EMOTE);
    data << uint32(TEXT_EMOTE_SALUTE);
    data << EmoteActionBase::GetNumberOfEmoteVariants(TEXT_EMOTE_SALUTE, bot->getRace(), bot->getGender());
    data << targetGuid;
    bot->GetSession()->HandleTextEmoteOpcode(data);

    bot->SetSelection(oldSelection);
    return true;
}
