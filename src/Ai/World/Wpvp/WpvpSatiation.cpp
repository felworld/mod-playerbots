#include "WpvpSatiation.h"

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
// 64 bit FNV-1a pair hash, same scheme as WpvpTruce.
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
}

bool WpvpSatiated(Player* bot, Player* enemy)
{
    if (!sPlayerbotAIConfig.wpvpSatiationChance)
        return false;

    return WpvpSatiationBoard::instance().IsSatiated(bot->GetGUID(), enemy->GetGUID());
}

void WpvpSatiationBoard::RecordKill(Player* killer, Player* victim)
{
    uint32 const chance = std::min<uint32>(sPlayerbotAIConfig.wpvpSatiationChance, 100);
    if (!chance)
        return;

    // Only bots get satiated - a real player's patience is their own.
    if (!GET_PLAYERBOT_AI(killer))
        return;

    bool const satiated = urand(1, 100) <= chance;
    METRIC_VALUE("playerbots_wpvp_satiation", 1, METRIC_TAG("outcome", satiated ? "satiated" : "hungry"));
    if (!satiated)
        return;

    LOG_DEBUG("playerbots", "Bot {} is satiated with {} for {} min", killer->GetName(), victim->GetName(),
              sPlayerbotAIConfig.wpvpSatiationMinutes);
    Felworld::LogEvent(killer->GetGUID(), "wpvp_satiated",
                       Acore::StringFormat("{{\"victim\":\"{}\"}}", victim->GetName()));

    uint32 const now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(now);
    _satiatedUntilMs[HashPair(killer->GetGUID().GetRawValue(), victim->GetGUID().GetRawValue())] =
        now + sPlayerbotAIConfig.wpvpSatiationMinutes * MINUTE * IN_MILLISECONDS;
}

bool WpvpSatiationBoard::IsSatiated(ObjectGuid killer, ObjectGuid victim)
{
    uint32 const now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _satiatedUntilMs.find(HashPair(killer.GetRawValue(), victim.GetRawValue()));
    return it != _satiatedUntilMs.end() && int32(it->second - now) > 0;
}

void WpvpSatiationBoard::Prune(uint32 now)
{
    for (auto it = _satiatedUntilMs.begin(); it != _satiatedUntilMs.end();)
    {
        if (int32(it->second - now) <= 0)
            it = _satiatedUntilMs.erase(it);
        else
            ++it;
    }
}
