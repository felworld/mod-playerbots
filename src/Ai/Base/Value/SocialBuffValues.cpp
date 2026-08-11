/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "SocialBuffValues.h"

#include "NonCombatActions.h"
#include "PaladinGreaterBlessingAction.h"
#include "Playerbots.h"

namespace
{
    constexpr uint32 PRUNE_INTERVAL_MS = 60 * 1000;

    // Check the class rather than the current power type so a druid in bear or
    // cat form still counts as a mana user.
    bool HasManaPool(Unit* target)
    {
        if (Player* player = target->ToPlayer())
            return player->getClass() != CLASS_WARRIOR && player->getClass() != CLASS_ROGUE &&
                   player->getClass() != CLASS_DEATH_KNIGHT;

        return target->getPowerType() == POWER_MANA;
    }

    // Attack power is dead weight for classes that never swing a weapon in
    // anger - the same three the greater-blessing assignment treats as pure
    // casters.
    bool BenefitsFromAttackPower(Unit* target)
    {
        Player* player = target->ToPlayer();
        if (!player)
            return true;

        switch (player->getClass())
        {
            case CLASS_MAGE:
            case CLASS_PRIEST:
            case CLASS_WARLOCK:
                return false;
            default:
                return true;
        }
    }

    // Only one blessing per paladin sticks, but blessings from different
    // paladins stack - so a passerby already carrying Might can still take
    // Kings from us. Walk the greater-blessing priority list for the target's
    // role and offer the best flavour they aren't already wearing, skipping
    // the two that are simply useless to their class.
    std::string SelectBlessingFor(PlayerbotAI* botAI, Unit* target)
    {
        using namespace ai::gbless;

        RoleProfile const role = ResolveRoleProfile(target->ToPlayer());
        for (BaseBlessingCategory const category : BASE_BLESSING_PRIORITIES[role].priorities)
        {
            if (category == BASE_NONE)
                continue;

            if (category == BASE_MIGHT && !BenefitsFromAttackPower(target))
                continue;

            if (category == BASE_WISDOM && !HasManaPool(target))
                continue;

            std::string const single = BlessingSpellName(ToSingleVariant(category));
            if (botAI->HasAura(single, target) || botAI->HasAura(BlessingSpellName(ToGreaterVariant(category)), target))
                continue;

            if (!botAI->CanCastSpell(single, target))
                continue;

            return single;
        }

        return "";
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
        // Unlike the one-buff classes above, a paladin has four flavours to
        // pick from, each with its own exclusion check.
        case CLASS_PALADIN:
            return SelectBlessingFor(botAI, target);
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

    // Finish the drink first: standing up to cast strips the seated regen
    // aura, and the eat/drink hold re-seats the bot before the cast can land,
    // livelocking both until the bar is full.
    if (BotConsumables::IsEatingFood(bot) || BotConsumables::IsDrinking(bot))
        return nullptr;

    if (bot->getPowerType() == POWER_MANA &&
        bot->GetPowerPct(POWER_MANA) < float(sPlayerbotAIConfig.socialBuffSelfMana))
        return nullptr;

    uint32 now = getMSTime();

    // Giver-side pacing: a real player might buff whoever happens to be next
    // to them, not methodically bless a whole plaza. One walk-up buff per
    // cooldown window; buff-backs bypass this value, so answering a buff is
    // never blocked (though it does restart the window).
    if (now < _giverReadyMs)
        return nullptr;

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
    uint32 now = getMSTime();
    _cooldownEndMs[targetGuid] = now + sPlayerbotAIConfig.socialBuffCooldown * IN_MILLISECONDS;
    _giverReadyMs = now + sPlayerbotAIConfig.socialBuffGiverCooldown * IN_MILLISECONDS;
}
