#ifndef PLAYERBOTS_DETERMINISTICROLL_H
#define PLAYERBOTS_DETERMINISTICROLL_H

#include <ctime>

#include "Define.h"

// Deterministic percent dice (Felworld), same FNV-1a scheme as
// PossibleTargetsValue's attack-decision hash: two GUIDs, a per-decision salt
// and a coarse time window diffuse into a stable 0-99 roll, so a choice about
// a pair holds for the whole window instead of flickering tick to tick. The
// salt keeps independent decisions about the same pair (join their fight?
// support them?) from always landing on the same side.
// See: http://www.isthe.com/chongo/tech/comp/fnv/
inline bool DeterministicRollPasses(uint64 first, uint64 second, uint64 salt, uint32 windowSeconds,
                                    uint32 chancePct)
{
    if (chancePct >= 100)
        return true;

    if (!chancePct)
        return false;

    constexpr uint64 FNV_OFFSET_BASIS = 14695981039346656037ULL;
    constexpr uint64 FNV_PRIME = 1099511628211ULL;

    uint64 hash = FNV_OFFSET_BASIS;
    hash ^= first;
    hash *= FNV_PRIME;
    hash ^= second;
    hash *= FNV_PRIME;
    hash ^= salt;
    hash *= FNV_PRIME;
    hash ^= static_cast<uint64>(time(nullptr) / windowSeconds);
    hash *= FNV_PRIME;

    return (hash % 100) < chancePct;
}

#endif
