/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "StealthReactValues.h"

#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include "Playerbots.h"
#include "SpellAuraDefines.h"

namespace
{
    constexpr uint32 PRUNE_INTERVAL_MS = 60 * 1000;
}

bool CanDetectStealth360(Player* bot, Unit* target)
{
    if (!target->m_stealth.GetFlags())
        return false;

    // Stealth layered under invisibility the bot can't pierce stays unseen.
    if (target->m_invisibility.GetFlags() && !bot->CanDetectInvisibilityOf(target))
        return false;

    float distance = bot->GetExactDist(target);
    float combatReach = bot->GetCombatReach();
    if (distance < combatReach)
        return true;

    for (uint32 i = 0; i < TOTAL_STEALTH_TYPES; ++i)
    {
        if (!(target->m_stealth.GetFlags() & (1 << i)))
            continue;

        if (bot->HasAuraTypeWithMiscvalue(SPELL_AURA_DETECT_STEALTH, i))
            return true;

        int32 detectionValue = 30;
        detectionValue += int32(bot->getLevelForTarget(target) - 1) * 5;
        detectionValue += bot->m_stealthDetect.GetValue(StealthType(i));
        detectionValue -= target->m_stealth.GetValue(StealthType(i));

        float visibilityRange = float(detectionValue) * 0.3f + combatReach;
        if (visibilityRange > MAX_PLAYER_STEALTH_DETECT_RANGE)
            visibilityRange = MAX_PLAYER_STEALTH_DETECT_RANGE;

        if (distance > visibilityRange)
            return false;
    }

    return true;
}

Unit* StealtherSpottedValue::Calculate()
{
    if (!sPlayerbotAIConfig.enableStealthReactions)
        return nullptr;

    if (!bot->IsAlive() || bot->IsInCombat())
        return nullptr;

    // A bot that is itself hiding doesn't twitch and give its position away.
    if (bot->HasStealthAura())
        return nullptr;

    uint32 now = getMSTime();
    if (now - _lastPruneMs > PRUNE_INTERVAL_MS)
    {
        _lastPruneMs = now;
        for (auto it = _cooldownEndMs.begin(); it != _cooldownEndMs.end();)
        {
            if (now >= it->second)
                it = _cooldownEndMs.erase(it);
            else
                ++it;
        }
    }

    std::list<Player*> players;
    Acore::AnyPlayerInObjectRangeCheck check(bot, MAX_PLAYER_STEALTH_DETECT_RANGE, /*reqAlive*/ true, /*disallowGM*/ true);
    Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(bot, players, check);
    Cell::VisitObjects(bot, searcher, MAX_PLAYER_STEALTH_DETECT_RANGE);

    Player* best = nullptr;
    float bestDist = 0.0f;

    for (Player* player : players)
    {
        if (player == bot || !player->HasStealthAura())
            continue;

        // Group members are mutually visible while stealthed
        // (Player::IsAlwaysDetectableFor) - seeing them isn't a detection.
        if (player->IsInSameRaidWith(bot))
            continue;

        auto cooldown = _cooldownEndMs.find(player->GetGUID());
        if (cooldown != _cooldownEndMs.end() && now < cooldown->second)
            continue;

        if (!CanDetectStealth360(bot, player))
            continue;

        // No heart-jump through a wall.
        if (!bot->IsWithinLOSInMap(player))
            continue;

        float dist = bot->GetExactDist(player);
        if (!best || dist < bestDist)
        {
            best = player;
            bestDist = dist;
        }
    }

    return best;
}

void StealtherSpottedValue::MarkReacted(ObjectGuid stealtherGuid)
{
    _cooldownEndMs[stealtherGuid] = getMSTime() + sPlayerbotAIConfig.stealthReactionCooldown * IN_MILLISECONDS;
}
