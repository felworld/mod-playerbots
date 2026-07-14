/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "BystanderValues.h"

#include "BystanderDistress.h"
#include "Playerbots.h"

#include <unordered_set>

namespace
{
    // Samples younger than this refresh in place instead of growing the
    // history; keeps the rate baseline meaningfully older than "this tick".
    constexpr uint32 SAMPLE_SPACING_MS = 1000;
    constexpr uint32 PRUNE_INTERVAL_MS = 10 * 1000;

    BystanderDistressConfig DistressConfig()
    {
        BystanderDistressConfig config;
        config.distressHealth = sPlayerbotAIConfig.bystanderDistressHealth;
        config.distressHealerHealth = sPlayerbotAIConfig.bystanderDistressHealerHealth;
        config.rateHealthLoss = sPlayerbotAIConfig.bystanderDistressRateHealthLoss;
        config.rateWindowMs = sPlayerbotAIConfig.bystanderDistressRateWindow * IN_MILLISECONDS;
        config.distressMobCount = sPlayerbotAIConfig.bystanderDistressMobCount;
        config.swarmHealthCeiling = 85;
        config.lowMana = sPlayerbotAIConfig.bystanderDistressLowMana;
        config.lowManaHealth = sPlayerbotAIConfig.bystanderDistressLowManaHealth;
        return config;
    }

    std::vector<Unit*> LivingCreatureAttackers(Player* victim)
    {
        std::vector<Unit*> attackers;
        for (Unit* attacker : victim->getAttackers())
            if (attacker && attacker->IsAlive() && attacker->IsCreature())
                attackers.push_back(attacker);

        return attackers;
    }

    // Casting on a PvP-flagged unit, or assisting one mid-PvP, flags the
    // caster (Spell::DoAllEffectOnTarget) - never let an unflagged bot do it.
    bool CanHealWithoutFlagging(Player* bot, Player* victim)
    {
        return bot->IsPvP() || (!victim->IsPvP() && !victim->HasUnitState(UNIT_STATE_ATTACK_PLAYER));
    }

    // PvP-flagged creatures (enemy faction guards) flag their attacker too.
    bool CanAttackWithoutFlagging(Player* bot, Unit* attacker)
    {
        return bot->IsValidAttackTarget(attacker) && (bot->IsPvP() || !attacker->IsPvP());
    }
}

bool IsBystanderHealerClass(Player* player)
{
    switch (player->getClass())
    {
        case CLASS_PRIEST:
        case CLASS_PALADIN:
        case CLASS_DRUID:
        case CLASS_SHAMAN:
            return true;
        default:
            return false;
    }
}

Unit* BystanderToAssistValue::Calculate()
{
    if (!sPlayerbotAIConfig.enableBystanderAssist)
        return nullptr;

    uint32 now = getMSTime();
    Prune(now);

    // v1: a bot only volunteers while free and in good shape - grouped bots
    // have a party to answer to, in-combat bots have their own problems.
    if (bot->GetGroup() || bot->IsInCombat())
        return nullptr;

    if (bot->GetHealthPct() < float(sPlayerbotAIConfig.bystanderAssistSelfHealth))
        return nullptr;

    if (bot->getPowerType() == POWER_MANA &&
        bot->GetPowerPct(POWER_MANA) < float(sPlayerbotAIConfig.bystanderAssistSelfMana))
        return nullptr;

    bool healerBot = IsBystanderHealerClass(bot);
    BystanderDistressConfig config = DistressConfig();

    Player* best = nullptr;
    float bestHealthPct = 101.0f;

    for (ObjectGuid const guid : AI_VALUE(GuidVector, "nearest friendly players"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Player* player = unit ? unit->ToPlayer() : nullptr;
        if (!player || player == bot || !player->IsAlive() || player->IsGameMaster())
            continue;

        // Sample everyone in sight, not just candidates: the rate baseline
        // must already exist by the time someone gets in trouble.
        SampleHealth(player, now);

        if (bot->GetDistance(player) > sPlayerbotAIConfig.bystanderAssistRadius)
            continue;

        auto cooldown = _cooldownEndMs.find(guid);
        if (cooldown != _cooldownEndMs.end() && now < cooldown->second)
            continue;

        if (!player->IsInCombat())
            continue;

        std::vector<Unit*> attackers = LivingCreatureAttackers(player);
        if (attackers.empty())
            continue;

        // Path-aware PvP safety: only pick victims this bot can legally help.
        if (healerBot)
        {
            if (!CanHealWithoutFlagging(bot, player))
                continue;
        }
        else
        {
            bool anyAttackable = false;
            for (Unit* attacker : attackers)
                if (CanAttackWithoutFlagging(bot, attacker))
                {
                    anyAttackable = true;
                    break;
                }

            if (!anyAttackable)
                continue;
        }

        // Anti-dogpile: stand down when enough others are already on it.
        std::unordered_set<ObjectGuid> helpers;
        for (Unit* attacker : attackers)
            for (Unit* helper : attacker->getAttackers())
                if (helper && helper != player && helper->IsPlayer())
                    helpers.insert(helper->GetGUID());

        if (helpers.size() >= sPlayerbotAIConfig.bystanderAssistMaxHelpers)
            continue;

        BystanderSnapshot snapshot = {};
        snapshot.healthPct = uint8(player->GetHealthPct());
        snapshot.hasMana = player->getPowerType() == POWER_MANA;
        snapshot.manaPct = uint8(player->GetPowerPct(POWER_MANA));
        snapshot.isHealerCapableClass = IsBystanderHealerClass(player);
        snapshot.inCombat = true;
        snapshot.creatureAttackerCount = uint8(std::min<size_t>(attackers.size(), 255));

        uint8 prevHealthPct = 0;
        uint32 prevAgeMs = 0;
        snapshot.hasPrevSample = FindPreviousSample(guid, now, prevHealthPct, prevAgeMs);
        snapshot.prevHealthPct = prevHealthPct;
        snapshot.prevSampleAgeMs = prevAgeMs;

        if (!IsBystanderInDistress(snapshot, config))
            continue;

        if (!IsWinnable(player, attackers))
            continue;

        if (player->GetHealthPct() < bestHealthPct)
        {
            best = player;
            bestHealthPct = player->GetHealthPct();
        }
    }

    return best;
}

void BystanderToAssistValue::MarkAssisted(ObjectGuid victimGuid)
{
    _cooldownEndMs[victimGuid] = getMSTime() + sPlayerbotAIConfig.bystanderAssistCooldown * IN_MILLISECONDS;
}

void BystanderToAssistValue::SampleHealth(Player* player, uint32 now)
{
    std::deque<HealthSample>& history = _samples[player->GetGUID()];
    if (!history.empty() && now - history.back().timeMs < SAMPLE_SPACING_MS)
        return;

    history.push_back({ uint8(player->GetHealthPct()), now });

    uint32 windowMs = sPlayerbotAIConfig.bystanderDistressRateWindow * IN_MILLISECONDS;
    while (!history.empty() && now - history.front().timeMs > windowMs)
        history.pop_front();
}

bool BystanderToAssistValue::FindPreviousSample(ObjectGuid guid, uint32 now, uint8& prevHealthPct,
                                                uint32& prevAgeMs) const
{
    auto it = _samples.find(guid);
    if (it == _samples.end() || it->second.empty())
        return false;

    // SampleHealth already discarded everything older than the rate window,
    // so the oldest remaining sample is the best baseline.
    HealthSample const& oldest = it->second.front();
    prevHealthPct = oldest.healthPct;
    prevAgeMs = now - oldest.timeMs;
    return true;
}

void BystanderToAssistValue::Prune(uint32 now)
{
    if (now - _lastPruneMs < PRUNE_INTERVAL_MS)
        return;

    _lastPruneMs = now;

    for (auto it = _samples.begin(); it != _samples.end();)
    {
        if (it->second.empty() || now - it->second.back().timeMs > PRUNE_INTERVAL_MS)
            it = _samples.erase(it);
        else
            ++it;
    }

    for (auto it = _cooldownEndMs.begin(); it != _cooldownEndMs.end();)
    {
        if (now >= it->second)
            it = _cooldownEndMs.erase(it);
        else
            ++it;
    }
}

bool BystanderToAssistValue::IsWinnable(Player* victim, std::vector<Unit*> const& attackers) const
{
    int32 botLevel = bot->GetLevel();
    uint32 foePower = 0;

    for (Unit* attacker : attackers)
    {
        // Solo bots have no business fighting elites or mobs far above them.
        if (int32(attacker->GetLevel()) - botLevel > 4)
            return false;

        if (Creature* creature = attacker->ToCreature())
            if (creature->GetCreatureTemplate()->rank > CREATURE_ELITE_NORMAL)
                return false;

        foePower += BystanderFoePower(int32(attacker->GetLevel()) - botLevel);
    }

    uint32 friendPower = BYSTANDER_SELF_BASE_POWER +
                         BystanderFriendPower(int32(victim->GetLevel()) - botLevel);

    for (ObjectGuid const guid : AI_VALUE(GuidVector, "nearest friendly players"))
    {
        Unit* helper = botAI->GetUnit(guid);
        if (!helper || helper == bot || helper == victim || !helper->IsAlive())
            continue;

        if (helper->GetDistance(victim) < 10.0f)
            friendPower += BystanderFriendPower(int32(helper->GetLevel()) - botLevel);
    }

    return foePower <= friendPower;
}

Unit* BystanderAttackerValue::Calculate()
{
    Unit* victim = AI_VALUE(Unit*, "bystander to assist");
    if (!victim)
        return nullptr;

    Unit* closest = nullptr;
    float closestDist = sPlayerbotAIConfig.bystanderAssistRadius;

    for (Unit* attacker : victim->getAttackers())
    {
        if (!attacker || !attacker->IsAlive() || !attacker->IsCreature())
            continue;

        if (!CanAttackWithoutFlagging(bot, attacker))
            continue;

        float dist = bot->GetDistance(attacker);
        if (dist < closestDist)
        {
            closest = attacker;
            closestDist = dist;
        }
    }

    return closest;
}
