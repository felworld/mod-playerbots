/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "StealthReactValues.h"

#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"
#include "SpellAuraDefines.h"
#include "WpvpSatiation.h"
#include "WpvpTruce.h"

namespace
{
    constexpr uint32 PRUNE_INTERVAL_MS = 60 * 1000;

    // How far around the bot the suspicion tracker keeps its perception
    // ledger - the "on screen" bubble a player would remember faces from.
    constexpr float STEALTH_TRACK_RANGE = 60.0f;

    // A ledger entry whose player drifted this far away is forgotten
    // without suspicion - they left, they didn't hide.
    constexpr float STEALTH_TRACK_DROP_RANGE = 90.0f;

    // Spacing between flush casts against one suspicion.
    constexpr uint32 STEALTH_FLUSH_RECAST_MS = 4 * 1000;

    // Whose disappearance would this bot even care about: enemies, plus a
    // duel opponent (who is usually same-faction and never PvP-flagged
    // against the bot).
    bool IsSuspect(Player* bot, PlayerbotAI* botAI, Player* player)
    {
        if (bot->duel && bot->duel->Opponent == player)
            return true;

        return botAI->IsOpposing(player);
    }

    // Whether the bot currently perceives this player at all: plain sight
    // through the server's visibility, or the 360° stealth-detection ping.
    bool Perceivable(Player* bot, Player* target)
    {
        if (!bot->IsWithinLOSInMap(target))
            return false;

        if (target->HasStealthAura())
            return CanDetectStealth360(bot, target);

        return bot->CanSeeOrDetect(target);
    }

    // The same courtesies that gate initiating an open attack gate hunting
    // a hidden one - flushing someone the bot wouldn't fight is griefing.
    bool WouldFlush(Player* bot, PlayerbotAI* botAI, Player* enemy)
    {
        // Except in a duel: the opponent hiding is the one case the bot is
        // expected to counter.
        if (bot->duel && bot->duel->Opponent == enemy)
            return true;

        if (!botAI->IsOpposing(enemy) || !enemy->IsPvP())
            return false;

        if (sPlayerbotAIConfig.IsPvpProhibited(enemy->GetZoneId(), enemy->GetAreaId()))
            return false;

        return !WpvpTruceHolds(bot, enemy) && !WpvpSatiated(bot, enemy);
    }
}

bool CanDetectStealth360(Player* bot, Unit* target)
{
    if (!target->m_stealth.GetFlags())
        return false;

    // Stealth layered under invisibility the bot can't pierce stays unseen -
    // an inline mirror of WorldObject::CanDetectInvisibilityOf (private to
    // the core), minus the invisible-seer corner: a bot startling in the
    // open is never itself invisible.
    if (uint32 invisFlags = target->m_invisibility.GetFlags())
    {
        uint32 mask = invisFlags & (bot->m_invisibilityDetect.GetFlags() | bot->m_invisibility.GetFlags());
        if (mask != invisFlags)
            return false;

        for (uint32 i = 0; i < TOTAL_INVISIBILITY_TYPES; ++i)
        {
            if (!(mask & (1 << i)))
                continue;

            // Visible for the same invisibility type.
            if (bot->m_invisibility.GetValue(InvisibilityType(i)) && target->m_invisibility.GetValue(InvisibilityType(i)))
                continue;

            if (bot->m_invisibilityDetect.GetValue(InvisibilityType(i)) < target->m_invisibility.GetValue(InvisibilityType(i)))
                return false;
        }
    }

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

float StealthDetectionRange(Unit const* seer, Unit const* stealther)
{
    // Detection needs the stealther within range for every flagged stealth
    // type (CanDetectStealthOf bails on the first type out of range), so
    // the effective ring is the minimum over the types.
    float range = 0.0f;

    for (uint32 i = 0; i < TOTAL_STEALTH_TYPES; ++i)
    {
        if (!(stealther->m_stealth.GetFlags() & (1 << i)))
            continue;

        if (seer->HasAuraTypeWithMiscvalue(SPELL_AURA_DETECT_STEALTH, i))
            return MAX_PLAYER_STEALTH_DETECT_RANGE;

        int32 detectionValue = 30;
        detectionValue += int32(seer->getLevelForTarget(stealther) - 1) * 5;
        detectionValue += seer->m_stealthDetect.GetValue(StealthType(i));
        detectionValue -= stealther->m_stealth.GetValue(StealthType(i));

        float visibilityRange = float(detectionValue) * 0.3f + seer->GetCombatReach();
        if (seer->IsPlayer() && visibilityRange > MAX_PLAYER_STEALTH_DETECT_RANGE)
            visibilityRange = MAX_PLAYER_STEALTH_DETECT_RANGE;

        if (range == 0.0f || visibilityRange < range)
            range = visibilityRange;
    }

    // Point-blank the seer always sees through stealth, whatever the math.
    return std::max(range, seer->GetCombatReach());
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

StealthSuspicion StealthSuspicionValue::Calculate()
{
    if (!sPlayerbotAIConfig.enableStealthReactions || !sPlayerbotAIConfig.stealthFlushChance)
        return StealthSuspicion();

    if (!bot->IsInWorld() || !bot->IsAlive())
    {
        _perceived.clear();
        _suspicion = StealthSuspicion();
        return _suspicion;
    }

    uint32 now = getMSTime();

    // Refresh the perception ledger from the players around the bot.
    std::list<Player*> players;
    Acore::AnyPlayerInObjectRangeCheck check(bot, STEALTH_TRACK_RANGE, /*reqAlive*/ true, /*disallowGM*/ true);
    Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(bot, players, check);
    Cell::VisitObjects(bot, searcher, STEALTH_TRACK_RANGE);

    for (Player* player : players)
    {
        if (player == bot || !IsSuspect(bot, botAI, player))
            continue;

        if (Perceivable(bot, player))
        {
            Perceived& entry = _perceived[player->GetGUID()];
            entry.pos = player->GetPosition();
            entry.lastSeenMs = now;
        }
    }

    // Entries the scan didn't refresh: decide "got away in the open"
    // (forget) from "went into hiding" (suspicion).
    for (auto it = _perceived.begin(); it != _perceived.end();)
    {
        if (it->second.lastSeenMs == now)
        {
            ++it;
            continue;
        }

        Player* player = ObjectAccessor::FindPlayer(it->first);
        if (!player || !player->IsAlive() || player->GetMap() != bot->GetMap() ||
            bot->GetExactDist(player) > STEALTH_TRACK_DROP_RANGE)
        {
            it = _perceived.erase(it);
            continue;
        }

        // Still perceivable, just outside the scan bubble - keep tracking.
        if (Perceivable(bot, player))
        {
            it->second.pos = player->GetPosition();
            it->second.lastSeenMs = now;
            ++it;
            continue;
        }

        if ((player->HasStealthAura() || player->HasInvisibilityAura()) && WouldFlush(bot, botAI, player))
        {
            _suspicion.stealther = it->first;
            _suspicion.stealtherName = player->GetName();
            _suspicion.lastKnown = it->second.pos;
            _suspicion.timeMs = now;
            _suspicion.flushApproved = urand(1, 100) <= sPlayerbotAIConfig.stealthFlushChance;

            LOG_DEBUG("playerbots", "Bot {} suspects {} went into hiding nearby (flush: {})", bot->GetName(),
                      _suspicion.stealtherName, _suspicion.flushApproved);
        }

        it = _perceived.erase(it);
    }

    if (_suspicion.timeMs)
    {
        bool const expired =
            getMSTimeDiff(_suspicion.timeMs, now) > sPlayerbotAIConfig.stealthFlushSeconds * IN_MILLISECONDS;

        if (_suspicion.stealther.IsEmpty())
        {
            // Seeded by inference (Distract): nobody to re-perceive, so
            // only the timer clears it.
            if (expired)
                _suspicion = StealthSuspicion();
        }
        else
        {
            Player* stealther = ObjectAccessor::FindPlayer(_suspicion.stealther);

            // Perceivable again means direct targeting is back on the table -
            // area flushing has done its job (or the stealther blew it).
            if (expired || !stealther || !stealther->IsAlive() || stealther->GetMap() != bot->GetMap() ||
                Perceivable(bot, stealther))
                _suspicion = StealthSuspicion();
        }
    }

    return _suspicion;
}

void StealthSuspicionValue::SeedSuspicion(Position const& spot)
{
    _suspicion.stealther = ObjectGuid::Empty;
    _suspicion.stealtherName = "someone unseen";
    _suspicion.lastKnown = spot;
    _suspicion.timeMs = getMSTime();
    _suspicion.flushApproved = urand(1, 100) <= sPlayerbotAIConfig.stealthFlushChance;
}

bool StealthSuspicionValue::FlushCastReady() const
{
    return !_lastFlushMs || getMSTimeDiff(_lastFlushMs, getMSTime()) >= STEALTH_FLUSH_RECAST_MS;
}

void StealthSuspicionValue::MarkFlushCast()
{
    _lastFlushMs = getMSTime();
}
