/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "SocialBuffValues.h"

#include "Playerbots.h"

namespace
{
    constexpr uint32 PRUNE_INTERVAL_MS = 60 * 1000;

    // A blessing slot is exclusive: carrying any of these means a paladin
    // has already been generous, whichever flavour it was.
    std::vector<std::string> const anyBlessing = {
        "blessing of might", "greater blessing of might",
        "blessing of wisdom", "greater blessing of wisdom",
        "blessing of kings", "greater blessing of kings",
        "blessing of sanctuary", "greater blessing of sanctuary",
    };

    // Check the class rather than the current power type so a druid in bear or
    // cat form still counts as a mana user.
    bool HasManaPool(Unit* target)
    {
        if (Player* player = target->ToPlayer())
            return player->getClass() != CLASS_WARRIOR && player->getClass() != CLASS_ROGUE &&
                   player->getClass() != CLASS_DEATH_KNIGHT;

        return target->getPowerType() == POWER_MANA;
    }
}

bool CanBuffWithoutFlagging(Player* bot, Unit* target)
{
    return bot->IsPvP() || (!target->IsPvP() && !target->HasUnitState(UNIT_STATE_ATTACK_PLAYER));
}

std::string SelectSocialBuffFor(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    std::string candidate;
    std::vector<std::string> const* excludes = nullptr;

    switch (bot->getClass())
    {
        case CLASS_MAGE:
        {
            // Intellect is useless to classes without a mana pool.
            if (!HasManaPool(target))
                return "";

            static std::vector<std::string> const has = { "arcane intellect", "arcane brilliance" };
            candidate = "arcane intellect";
            excludes = &has;
            break;
        }
        case CLASS_PRIEST:
        {
            static std::vector<std::string> const has = { "power word: fortitude", "prayer of fortitude" };
            candidate = "power word: fortitude";
            excludes = &has;
            break;
        }
        case CLASS_DRUID:
        {
            static std::vector<std::string> const has = { "mark of the wild", "gift of the wild" };
            candidate = "mark of the wild";
            excludes = &has;
            break;
        }
        case CLASS_PALADIN:
        {
            candidate = target->getPowerType() == POWER_MANA ? "blessing of wisdom" : "blessing of might";
            excludes = &anyBlessing;
            break;
        }
        default:
            return "";
    }

    for (std::string const& aura : *excludes)
        if (botAI->HasAura(aura, target))
            return "";

    if (!botAI->CanCastSpell(candidate, target))
        return "";

    return candidate;
}

Unit* PasserbyToBuffValue::Calculate()
{
    if (!sPlayerbotAIConfig.enableSocialBuffing)
        return nullptr;

    // Like bystander assist v1: only free, comfortable bots volunteer.
    if (bot->GetGroup() || bot->IsInCombat() || !bot->IsAlive())
        return nullptr;

    if (bot->getPowerType() == POWER_MANA &&
        bot->GetPowerPct(POWER_MANA) < float(sPlayerbotAIConfig.socialBuffSelfMana))
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

    Unit* best = nullptr;
    float bestDist = sPlayerbotAIConfig.socialBuffRadius;

    for (ObjectGuid const guid : AI_VALUE(GuidVector, "nearest friendly players"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Player* player = unit ? unit->ToPlayer() : nullptr;
        if (!player || player == bot || !player->IsAlive() || player->IsGameMaster())
            continue;

        // Someone fighting for their life wants a rescue (bystander assist),
        // not a blessing.
        if (player->IsInCombat())
            continue;

        float dist = bot->GetDistance(player);
        if (dist >= bestDist)
            continue;

        auto cooldown = _cooldownEndMs.find(guid);
        if (cooldown != _cooldownEndMs.end() && now < cooldown->second)
            continue;

        if (!CanBuffWithoutFlagging(bot, player))
            continue;

        if (SelectSocialBuffFor(botAI, bot, player).empty())
            continue;

        best = player;
        bestDist = dist;
    }

    return best;
}

void PasserbyToBuffValue::MarkBuffed(ObjectGuid targetGuid)
{
    _cooldownEndMs[targetGuid] = getMSTime() + sPlayerbotAIConfig.socialBuffCooldown * IN_MILLISECONDS;
}
