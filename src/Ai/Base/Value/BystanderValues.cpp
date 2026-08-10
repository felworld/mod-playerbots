/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "BystanderValues.h"

#include "BystanderDistress.h"
#include "DeterministicRoll.h"
#include "LevelPerception.h"
#include "Playerbots.h"

#include <unordered_set>

namespace
{
    // Samples younger than this refresh in place instead of growing the
    // history; keeps the rate baseline meaningfully older than "this tick".
    constexpr uint32 SAMPLE_SPACING_MS = 1000;
    constexpr uint32 PRUNE_INTERVAL_MS = 10 * 1000;

    // PvP support dice (issue #53): stable per bot/victim pair for the same
    // window as the other deterministic PvP decisions.
    constexpr uint64 PVP_SUPPORT_ROLL_SALT = 0x4845414c;  // 'HEAL'
    constexpr uint32 PVP_SUPPORT_ROLL_WINDOW = 2 * MINUTE;

    // A supporter can out-heal one or two enemies' damage; healing into a
    // squad stomp just gets the healer killed too - the PvP analog of the
    // creature-fight winnability estimate below.
    constexpr size_t MAX_SUPPORTABLE_PLAYER_ATTACKERS = 2;

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

    std::vector<Unit*> LivingPlayerAttackers(Player* victim)
    {
        std::vector<Unit*> attackers;
        for (Unit* attacker : victim->getAttackers())
            if (attacker && attacker->IsAlive() && attacker->IsPlayer())
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

    // A bot only volunteers while free and in good shape - grouped bots have
    // a party to answer to. In combat nobody new gets adopted; a healer
    // mid-rescue keeps its victim (the first support heal is what put it in
    // combat).
    if (bot->GetGroup())
        return nullptr;

    if (bot->IsInCombat())
        return IsBystanderHealerClass(bot) ? SustainTarget() : nullptr;

    _sustainVictim.Clear();

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

        std::vector<Unit*> creatureAttackers = LivingCreatureAttackers(player);
        std::vector<Unit*> playerAttackers = LivingPlayerAttackers(player);
        if (creatureAttackers.empty() && playerAttackers.empty())
            continue;

        // Path-aware PvP safety: only pick victims this bot can legally help.
        if (healerBot)
        {
            if (!CanHealWithoutFlagging(bot, player))
                continue;
        }
        else
        {
            // Non-healers help by attacking a creature attacker. A victim
            // under player attack only is not theirs to rescue here: joining
            // the fight is passerby assist's job (EnemyPlayerValue).
            bool anyAttackable = false;
            for (Unit* attacker : creatureAttackers)
                if (CanAttackWithoutFlagging(bot, attacker))
                {
                    anyAttackable = true;
                    break;
                }

            if (!anyAttackable)
                continue;
        }

        // PvP support dice (issue #53): not every passerby commits to
        // someone else's player fight - each would-be supporter rolls once
        // per victim per window.
        if (creatureAttackers.empty() &&
            !DeterministicRollPasses(bot->GetGUID().GetRawValue(), guid.GetRawValue(),
                                     PVP_SUPPORT_ROLL_SALT, PVP_SUPPORT_ROLL_WINDOW,
                                     sPlayerbotAIConfig.bystanderPvpSupportChance))
            continue;

        // Anti-dogpile: stand down when enough others are already on it.
        std::unordered_set<ObjectGuid> helpers;
        for (Unit* attacker : creatureAttackers)
            for (Unit* helper : attacker->getAttackers())
                if (helper && helper != player && helper->IsPlayer())
                    helpers.insert(helper->GetGUID());

        for (Unit* attacker : playerAttackers)
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
        snapshot.creatureAttackerCount = uint8(std::min<size_t>(creatureAttackers.size(), 255));
        snapshot.playerAttackerCount = uint8(std::min<size_t>(playerAttackers.size(), 255));

        uint8 prevHealthPct = 0;
        uint32 prevAgeMs = 0;
        snapshot.hasPrevSample = FindPreviousSample(guid, now, prevHealthPct, prevAgeMs);
        snapshot.prevHealthPct = prevHealthPct;
        snapshot.prevSampleAgeMs = prevAgeMs;

        if (!IsBystanderInDistress(snapshot, config))
            continue;

        // Creature fights are judged by composition; player fights by
        // dogpile size - support goes to someone losing a fight, not into a
        // massacre-by-numbers.
        if (!creatureAttackers.empty() && !IsWinnable(player, creatureAttackers))
            continue;

        if (playerAttackers.size() > MAX_SUPPORTABLE_PLAYER_ATTACKERS)
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

    // Adopt for sustain: the assist that just fired may pull the bot into
    // combat, and the rescue continues through it.
    _sustainVictim = victimGuid;
}

// Mid-rescue: the victim adopted by the last assist, for as long as the
// rescue still makes sense - alive, still under attack, in range, legal to
// heal, and the bot itself still in shape to help. No dice, cooldowns or
// distress re-checks here: those gate ADOPTING a victim, not standing by one.
Unit* BystanderToAssistValue::SustainTarget()
{
    if (_sustainVictim.IsEmpty())
        return nullptr;

    Unit* unit = botAI->GetUnit(_sustainVictim);
    Player* victim = unit ? unit->ToPlayer() : nullptr;
    if (!victim || !victim->IsAlive() || !victim->IsInCombat() ||
        bot->GetDistance(victim) > sPlayerbotAIConfig.bystanderAssistRadius ||
        (LivingCreatureAttackers(victim).empty() && LivingPlayerAttackers(victim).empty()))
    {
        _sustainVictim.Clear();
        return nullptr;
    }

    // The same self-preservation gates as adoption: a healer taking a
    // beating (the enemy turned on it) or running dry stops supporting and
    // fights its own fight.
    if (bot->GetHealthPct() < float(sPlayerbotAIConfig.bystanderAssistSelfHealth))
        return nullptr;

    if (bot->getPowerType() == POWER_MANA &&
        bot->GetPowerPct(POWER_MANA) < float(sPlayerbotAIConfig.bystanderAssistSelfMana))
        return nullptr;

    if (!CanHealWithoutFlagging(bot, victim))
        return nullptr;

    return victim;
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
        // Mobs far above the bot are out of the question regardless of count.
        if (int32(PerceivedLevel(bot, attacker)) - botLevel > 4)
            return false;

        // Elites aren't excluded - two players often take one elite - they
        // just weigh in as several normal mobs (cf. AttackerCountValues'
        // rank weighting), so composition decides: victim + bot handle one
        // elite, two elites or an elite with adds tips the estimate over.
        uint32 rankFactorPct = 100;
        if (Creature* creature = attacker->ToCreature())
        {
            switch (creature->GetCreatureTemplate()->rank)
            {
                case CREATURE_ELITE_RARE:
                    rankFactorPct = 200;
                    break;
                case CREATURE_ELITE_ELITE:
                case CREATURE_ELITE_RAREELITE:
                    rankFactorPct = 300;
                    break;
                case CREATURE_ELITE_WORLDBOSS:
                    return false;
                default:
                    break;
            }
        }

        foePower += BystanderFoePower(int32(PerceivedLevel(bot, attacker)) - botLevel, rankFactorPct);
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
