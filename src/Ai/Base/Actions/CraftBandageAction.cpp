/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "CraftBandageAction.h"

#include "Bag.h"
#include "DBCStructure.h"
#include "Event.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <functional>
#include <vector>

namespace
{
    bool IsBandage(ItemTemplate const* proto)
    {
        return proto && proto->Class == ITEM_CLASS_CONSUMABLE && proto->SubClass == ITEM_SUBCLASS_BANDAGE;
    }

    void VisitBandages(Player* bot, std::function<void(Item*)> const& visit)
    {
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                if (IsBandage(item->GetTemplate()))
                    visit(item);

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
            if (Bag* bag = bot->GetBagByPos(bagSlot))
                for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                    if (Item* item = bag->GetItemByPos(slot))
                        if (IsBandage(item->GetTemplate()))
                            visit(item);
    }

    // A recipe is gray once the bot's First Aid skill reaches its trivial threshold.
    bool IsGrayRecipe(Player* bot, uint32 spellId)
    {
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
        {
            SkillLineAbilityEntry const* entry = itr->second;
            if (entry->SkillLine != SKILL_FIRST_AID)
                continue;

            if (entry->TrivialSkillLineRankHigh &&
                bot->GetSkillValue(SKILL_FIRST_AID) >= entry->TrivialSkillLineRankHigh)
                return true;
        }

        return false;
    }
}

uint32 CraftBandageAction::FindBestBandageSpell(Player* bot)
{
    struct Recipe
    {
        uint32 spellId;
        uint32 itemLevel;
        bool gray;
        bool hasReagents;
    };

    std::vector<Recipe> recipes;

    for (auto const& [spellId, playerSpell] : bot->GetSpellMap())
    {
        if (playerSpell->State == PLAYERSPELL_REMOVED || !playerSpell->Active)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || spellInfo->Effects[EFFECT_0].Effect != SPELL_EFFECT_CREATE_ITEM)
            continue;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(spellInfo->Effects[EFFECT_0].ItemType);
        if (!IsBandage(proto))
            continue;

        bool hasReagents = true;
        for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
        {
            if (spellInfo->Reagent[i] > 0 && !bot->HasItemCount(spellInfo->Reagent[i], spellInfo->ReagentCount[i]))
            {
                hasReagents = false;
                break;
            }
        }

        recipes.push_back({ spellId, proto->ItemLevel, IsGrayRecipe(bot, spellId), hasReagents });
    }

    uint32 highestKnownItemLevel = 0;
    for (auto const& recipe : recipes)
        highestKnownItemLevel = std::max(highestKnownItemLevel, recipe.itemLevel);

    uint32 bestSpellId = 0;
    uint32 bestItemLevel = 0;

    for (auto const& recipe : recipes)
    {
        if (!recipe.hasReagents)
            continue;

        // A gray recipe is only worth crafting when it is the best bandage the bot can ever make
        // (skill-capped bots must not go without; everyone else waits for level-appropriate cloth).
        if (recipe.gray && recipe.itemLevel < highestKnownItemLevel)
            continue;

        if (!bestSpellId || recipe.itemLevel > bestItemLevel)
        {
            bestSpellId = recipe.spellId;
            bestItemLevel = recipe.itemLevel;
        }
    }

    return bestSpellId;
}

uint32 CraftBandageAction::BandageItemLevel(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || spellInfo->Effects[EFFECT_0].Effect != SPELL_EFFECT_CREATE_ITEM)
        return 0;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(spellInfo->Effects[EFFECT_0].ItemType);
    return IsBandage(proto) ? proto->ItemLevel : 0;
}

uint32 CraftBandageAction::BandageCount(Player* bot)
{
    uint32 count = 0;
    VisitBandages(bot, [&count](Item* item) { count += item->GetCount(); });
    return count;
}

uint32 CraftBandageAction::LowerTierBandageCount(Player* bot, uint32 itemLevel)
{
    uint32 count = 0;
    VisitBandages(bot, [&count, itemLevel](Item* item)
    {
        if (item->GetTemplate()->ItemLevel < itemLevel)
            count += item->GetCount();
    });

    return count;
}

bool CraftBandageAction::isUseful()
{
    return botAI->HasSkill(SKILL_FIRST_AID) && !bot->IsInCombat() && !bot->isMoving();
}

bool CraftBandageAction::Execute(Event /*event*/)
{
    uint32 spellId = FindBestBandageSpell(bot);
    if (!spellId)
        return false;

    uint32 itemLevel = BandageItemLevel(spellId);

    // Evict bandages the bot has outgrown so they never shadow the good ones.
    std::vector<Item*> stale;
    VisitBandages(bot, [&stale, itemLevel](Item* item)
    {
        if (item->GetTemplate()->ItemLevel < itemLevel)
            stale.push_back(item);
    });

    for (Item* item : stale)
        bot->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);

    if (BandageCount(bot) >= CRAFT_BANDAGE_TARGET_COUNT)
        return !stale.empty();

    return botAI->CastSpell(spellId, bot);
}
