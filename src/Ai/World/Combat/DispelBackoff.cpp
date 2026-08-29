#include "DispelBackoff.h"

#include <algorithm>

#include "Log.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Unit.h"

namespace
{
// 64 bit FNV-1a hash over the (caster, target, school) triple, same scheme
// as WpvpSatiation's pair hash.
constexpr uint64 FNV_OFFSET_BASIS = 14695981039346656037ULL;
constexpr uint64 FNV_PRIME = 1099511628211ULL;

uint64 HashKey(uint64 caster, uint64 target, uint8 dispelType)
{
    uint64 hash = FNV_OFFSET_BASIS;
    hash ^= caster;
    hash *= FNV_PRIME;
    hash ^= target;
    hash *= FNV_PRIME;
    hash ^= dispelType;
    hash *= FNV_PRIME;
    return hash;
}

// Only the schools an opponent can actively cleanse; stealth, enrage and
// the other pseudo-schools never loop.
bool IsCleansableSchool(uint8 dispelType)
{
    return dispelType == DISPEL_MAGIC || dispelType == DISPEL_CURSE || dispelType == DISPEL_DISEASE ||
           dispelType == DISPEL_POISON;
}
}

bool DispelBackoffActive(Player* bot, Unit* target, uint32 spellId)
{
    if (!sPlayerbotAIConfig.debuffDispelBackoffCount || !spellId || !target)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !IsCleansableSchool(spellInfo->Dispel))
        return false;

    return DispelBackoffBoard::instance().IsBackedOff(bot->GetGUID(), target->GetGUID(), spellInfo->Dispel);
}

void DispelBackoffBoard::RecordDispel(Player* caster, Unit* target, uint8 dispelType)
{
    uint32 const count = sPlayerbotAIConfig.debuffDispelBackoffCount;
    if (!count || !IsCleansableSchool(dispelType))
        return;

    uint32 const now = getMSTime();
    uint32 const windowMs = sPlayerbotAIConfig.debuffDispelBackoffSeconds * IN_MILLISECONDS;

    std::lock_guard<std::mutex> lock(_mutex);

    // Cheap unbounded-growth backstop; entries rebuild harmlessly.
    if (_dispelTimesMs.size() > 4096)
        _dispelTimesMs.clear();

    std::vector<uint32>& times =
        _dispelTimesMs[HashKey(caster->GetGUID().GetRawValue(), target->GetGUID().GetRawValue(), dispelType)];
    times.erase(std::remove_if(times.begin(), times.end(), [&](uint32 ts) { return now - ts > windowMs; }),
                times.end());
    times.push_back(now);

    if (times.size() == count)
        LOG_DEBUG("playerbots", "Bot {} backing off dispel school {} debuffs on {} ({} dispels in {}s)",
                  caster->GetName(), dispelType, target->GetName(), count,
                  sPlayerbotAIConfig.debuffDispelBackoffSeconds);
}

bool DispelBackoffBoard::IsBackedOff(ObjectGuid caster, ObjectGuid target, uint8 dispelType)
{
    uint32 const now = getMSTime();
    uint32 const windowMs = sPlayerbotAIConfig.debuffDispelBackoffSeconds * IN_MILLISECONDS;

    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _dispelTimesMs.find(HashKey(caster.GetRawValue(), target.GetRawValue(), dispelType));
    if (it == _dispelTimesMs.end())
        return false;

    uint32 const recent = std::count_if(it->second.begin(), it->second.end(),
                                        [&](uint32 ts) { return now - ts <= windowMs; });
    return recent >= sPlayerbotAIConfig.debuffDispelBackoffCount;
}
