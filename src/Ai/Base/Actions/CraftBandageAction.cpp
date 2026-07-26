/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "CraftBandageAction.h"

#include "Bag.h"
#include "Event.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace
{
    bool IsBandage(ItemTemplate const* proto)
    {
        return proto && proto->Class == ITEM_CLASS_CONSUMABLE && proto->SubClass == ITEM_SUBCLASS_BANDAGE;
    }
}

uint32 CraftBandageAction::FindBestBandageSpell(Player* bot)
{
    uint32 bestSpellId = 0;
    uint32 bestItemLevel = 0;

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

        if (!hasReagents)
            continue;

        if (!bestSpellId || proto->ItemLevel > bestItemLevel)
        {
            bestSpellId = spellId;
            bestItemLevel = proto->ItemLevel;
        }
    }

    return bestSpellId;
}

uint32 CraftBandageAction::BandageCount(Player* bot)
{
    uint32 count = 0;

    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (IsBandage(item->GetTemplate()))
                count += item->GetCount();

    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        if (Bag* bag = bot->GetBagByPos(bagSlot))
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                if (Item* item = bag->GetItemByPos(slot))
                    if (IsBandage(item->GetTemplate()))
                        count += item->GetCount();

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

    return botAI->CastSpell(spellId, bot);
}
