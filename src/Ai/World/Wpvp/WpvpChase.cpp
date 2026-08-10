#include "WpvpChase.h"

#include <algorithm>

#include "FelworldEvents.h"
#include "Metric.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Random.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include "Timer.h"

namespace
{
// 64 bit FNV-1a pair hash, same scheme as WpvpSatiation.
constexpr uint64 FNV_OFFSET_BASIS = 14695981039346656037ULL;
constexpr uint64 FNV_PRIME = 1099511628211ULL;

uint64 HashPair(uint64 first, uint64 second)
{
    uint64 hash = FNV_OFFSET_BASIS;
    hash ^= first;
    hash *= FNV_PRIME;
    hash ^= second;
    hash *= FNV_PRIME;
    return hash;
}

// Within this range the pair counts as "in contact" even without damage
// landing - close enough that the fight is happening, not being chased.
constexpr float CONTACT_RANGE = 30.0f;

// Damage in either direction keeps contact alive this long.
constexpr uint32 DAMAGE_CONTACT_GRACE_MS = 5 * IN_MILLISECONDS;

// The runner must close this much inside the break-off distance before the
// approach reads as a re-entrance rather than position jitter.
constexpr float REENGAGE_MARGIN = 5.0f;

constexpr uint32 PURSUIT_STALE_MS = 2 * MINUTE * IN_MILLISECONDS;
constexpr uint32 BAN_STALE_MS = 10 * MINUTE * IN_MILLISECONDS;
constexpr uint32 DAMAGE_STALE_MS = 1 * MINUTE * IN_MILLISECONDS;

// The leash only concerns open-world cross-faction pursuit: battlegrounds,
// arenas, and the bot's own duel opponent play by their instanced rules.
bool EligiblePair(Player* bot, Player* enemy)
{
    if (enemy == bot || bot->InBattleground() || bot->InArena())
        return false;

    if (bot->duel && bot->duel->Opponent == enemy)
        return false;

    return bot->GetTeamId() != enemy->GetTeamId();
}

uint32 RollDelayMs()
{
    return urand(sPlayerbotAIConfig.wpvpChaseBreakSecondsMin, sPlayerbotAIConfig.wpvpChaseBreakSecondsMax) *
           IN_MILLISECONDS;
}
}

bool WpvpChaseBroken(Player* bot, Unit* target)
{
    // Mercy grants (WpvpGrudge) live in the same ban map, so the board stays
    // consulted even with the chase leash itself disabled.
    if (!sPlayerbotAIConfig.wpvpChaseBreakChance && !sPlayerbotAIConfig.wpvpBegMercyChance)
        return false;

    Player* enemy = target ? target->ToPlayer() : nullptr;
    if (!enemy || !EligiblePair(bot, enemy))
        return false;

    return WpvpChaseBoard::instance().UpdatePursuit(bot, enemy);
}

bool WpvpChaseBanned(Player* bot, Unit* target)
{
    if (!sPlayerbotAIConfig.wpvpChaseBreakChance && !sPlayerbotAIConfig.wpvpBegMercyChance)
        return false;

    Player* enemy = target ? target->ToPlayer() : nullptr;
    if (!enemy || !EligiblePair(bot, enemy))
        return false;

    return WpvpChaseBoard::instance().IsBanned(bot, enemy);
}

void WpvpChaseBoard::NoteDamage(Unit* attacker, Unit* victim)
{
    if (!sPlayerbotAIConfig.wpvpChaseBreakChance && !sPlayerbotAIConfig.wpvpBegMercyChance)
        return;

    if (!attacker || !victim)
        return;

    // Pets, totems, and guardians hit on their owner's behalf.
    Player* attackerPlayer = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
    Player* victimPlayer = victim->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!attackerPlayer || !victimPlayer || attackerPlayer == victimPlayer)
        return;

    if (attackerPlayer->InBattleground() || attackerPlayer->GetTeamId() == victimPlayer->GetTeamId())
        return;

    uint32 const now = getMSTime();
    uint64 const attackerGuid = attackerPlayer->GetGUID().GetRawValue();
    uint64 const victimGuid = victimPlayer->GetGUID().GetRawValue();

    std::lock_guard<std::mutex> lock(_mutex);
    if (_lastDamageMs.size() > 256)
        Prune(now);

    _lastDamageMs[HashPair(std::min(attackerGuid, victimGuid), std::max(attackerGuid, victimGuid))] = now;

    // A runner swinging at anyone re-arms every abandoned chase against them -
    // a witness sees them re-enter the fight, not slip away. The pair erase
    // covers the other direction (the attacker's own abandoned chase on the
    // player they just hit).
    for (auto it = _bans.begin(); it != _bans.end();)
    {
        if (it->second.targetRaw == attackerGuid)
            it = _bans.erase(it);
        else
            ++it;
    }
    _bans.erase(HashPair(attackerGuid, victimGuid));
}

bool WpvpChaseBoard::UpdatePursuit(Player* bot, Player* target)
{
    uint32 const now = getMSTime();
    float const distance = bot->GetDistance(target);
    uint64 const botGuid = bot->GetGUID().GetRawValue();
    uint64 const targetGuid = target->GetGUID().GetRawValue();
    uint64 const key = HashPair(botGuid, targetGuid);

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(now);

    // An already-abandoned chase stays broken; only the runner's own actions
    // (approach or damage) clear the ban.
    if (auto banIt = _bans.find(key); banIt != _bans.end())
    {
        banIt->second.touchedMs = now;
        return true;
    }

    // In mercy-only mode (leash disabled) the board still honors existing
    // bans above, but never runs break rolls.
    if (!sPlayerbotAIConfig.wpvpChaseBreakChance)
        return false;

    Pursuit& pursuit = _pursuits[key];
    pursuit.touchedMs = now;

    auto damageIt = _lastDamageMs.find(HashPair(std::min(botGuid, targetGuid), std::max(botGuid, targetGuid)));
    bool const inContact = distance <= CONTACT_RANGE ||
                           (damageIt != _lastDamageMs.end() && now - damageIt->second <= DAMAGE_CONTACT_GRACE_MS);
    if (inContact)
    {
        pursuit.nextRollMs = 0;
        return false;
    }

    if (!pursuit.nextRollMs)
    {
        pursuit.nextRollMs = now + RollDelayMs();
        return false;
    }

    if (int32(pursuit.nextRollMs - now) > 0)
        return false;

    bool const broken = urand(1, 100) <= sPlayerbotAIConfig.wpvpChaseBreakChance;
    METRIC_VALUE("playerbots_wpvp_chase_roll", 1, METRIC_TAG("outcome", broken ? "broken" : "dogged"));
    if (!broken)
    {
        pursuit.nextRollMs = now + RollDelayMs();
        return false;
    }

    LOG_DEBUG("playerbots", "Bot {} breaks off chasing {} at {:.0f}yd", bot->GetName(), target->GetName(), distance);
    Felworld::LogEvent(bot->GetGUID(), "wpvp_chase_broken",
                       Acore::StringFormat("{{\"target\":\"{}\",\"distance\":{:.0f}}}", target->GetName(), distance));

    _pursuits.erase(key);
    _bans[key] = {distance, now, targetGuid};
    return true;
}

void WpvpChaseBoard::GrantMercy(Player* bot, Player* beggar)
{
    uint32 const now = getMSTime();
    uint64 const key = HashPair(bot->GetGUID().GetRawValue(), beggar->GetGUID().GetRawValue());

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(now);

    _pursuits.erase(key);

    // Break distance 0: no approach of the beggar's ever reads as a
    // re-entrance - only their own swing (NoteDamage) or staleness clears.
    _bans[key] = {0.0f, now, beggar->GetGUID().GetRawValue()};
}

bool WpvpChaseBoard::IsBanned(Player* bot, Player* target)
{
    uint32 const now = getMSTime();
    float const distance = bot->GetDistance(target);
    uint64 const key = HashPair(bot->GetGUID().GetRawValue(), target->GetGUID().GetRawValue());

    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _bans.find(key);
    if (it == _bans.end())
        return false;

    // Coming back is an invitation: once the runner has closed meaningfully
    // inside where the bot gave up, the chase is back on.
    if (distance + REENGAGE_MARGIN < it->second.breakDistance)
    {
        LOG_DEBUG("playerbots", "Bot {} re-engages returning runner {}", bot->GetName(), target->GetName());
        _bans.erase(it);
        return false;
    }

    it->second.touchedMs = now;
    return true;
}

void WpvpChaseBoard::Prune(uint32 now)
{
    for (auto it = _lastDamageMs.begin(); it != _lastDamageMs.end();)
    {
        if (now - it->second > DAMAGE_STALE_MS)
            it = _lastDamageMs.erase(it);
        else
            ++it;
    }

    for (auto it = _pursuits.begin(); it != _pursuits.end();)
    {
        if (now - it->second.touchedMs > PURSUIT_STALE_MS)
            it = _pursuits.erase(it);
        else
            ++it;
    }

    for (auto it = _bans.begin(); it != _bans.end();)
    {
        if (now - it->second.touchedMs > BAN_STALE_MS)
            it = _bans.erase(it);
        else
            ++it;
    }
}
