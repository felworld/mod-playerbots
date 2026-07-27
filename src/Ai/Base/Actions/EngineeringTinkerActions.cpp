/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EngineeringTinkerActions.h"

#include "ChatHelper.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Event.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace
{
    bool IsSpeedBurstSpell(SpellInfo const* spellInfo)
    {
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (spellInfo->Effects[i].Effect == SPELL_EFFECT_APPLY_AURA &&
                spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_INCREASE_SPEED)
                return true;

        return false;
    }

    bool UseEquipped(Player* bot, PlayerbotAI* botAI, Item* item, Unit* unitTarget)
    {
        uint32 spellId = EngineeringTinkers::UseSpellId(item);
        if (!spellId)
            return false;

        uint8 bagIndex = item->GetBagSlot();
        uint8 slot = item->GetSlot();
        uint8 castCount = 1;
        ObjectGuid itemGuid = item->GetGUID();
        uint32 glyphIndex = 0;
        uint8 castFlags = 0;

        WorldPacket packet(CMSG_USE_ITEM);
        packet << bagIndex << slot << castCount << spellId << itemGuid << glyphIndex << castFlags;

        if (unitTarget)
        {
            packet << uint32(TARGET_FLAG_UNIT);
            packet << unitTarget->GetGUID().WriteAsPacked();
        }
        else
            packet << uint32(TARGET_FLAG_NONE);

        bot->GetSession()->HandleUseItemOpcode(packet);
        return true;
    }
}

namespace EngineeringTinkers
{
    uint32 UseSpellId(Item* item)
    {
        // A tinker on the permanent enchant slot wins over the item's own spell.
        if (uint32 enchantId = item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT))
            if (SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId))
                for (uint8 s = 0; s < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++s)
                    if (enchant->type[s] == ITEM_ENCHANTMENT_TYPE_USE_SPELL && enchant->spellid[s])
                        return enchant->spellid[s];

        ItemTemplate const* proto = item->GetTemplate();
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            if (proto->Spells[i].SpellId > 0 && proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
                return proto->Spells[i].SpellId;

        return 0;
    }

    Item* UsableEquipped(Player* bot, uint8 equipSlot, bool requireSpeedBurst)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, equipSlot);
        if (!item)
            return nullptr;

        uint32 spellId = UseSpellId(item);
        if (!spellId || bot->HasSpellCooldown(spellId))
            return nullptr;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || (requireSpeedBurst && !IsSpeedBurstSpell(spellInfo)))
            return nullptr;

        return item;
    }
}

bool UseRocketBootsAction::isPossible()
{
    return EngineeringTinkers::UsableEquipped(bot, EQUIPMENT_SLOT_FEET, true) != nullptr;
}

bool UseRocketBootsAction::Execute(Event /*event*/)
{
    Item* boots = EngineeringTinkers::UsableEquipped(bot, EQUIPMENT_SLOT_FEET, true);
    if (!boots || !UseEquipped(bot, botAI, boots, nullptr))
        return false;

    botAI->TellMasterNoFacing(PlayerbotTextMgr::instance().GetBotTextOrDefault(
        "use_rocket_boots", "Punching it!", {}));
    return true;
}

bool UseGloveTinkerAction::isUseful()
{
    return bot->IsInCombat() && botAI->DuelAllowsConsumable(DuelConsumables::BANDAGES);
}

bool UseGloveTinkerAction::isPossible()
{
    return EngineeringTinkers::UsableEquipped(bot, EQUIPMENT_SLOT_HANDS, false) != nullptr;
}

bool UseGloveTinkerAction::Execute(Event /*event*/)
{
    Item* gloves = EngineeringTinkers::UsableEquipped(bot, EQUIPMENT_SLOT_HANDS, false);
    if (!gloves)
        return false;

    uint32 spellId = EngineeringTinkers::UseSpellId(gloves);
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    // Offensive tinkers (pyro rocket) need an enemy in range; buffs fire on the spot.
    Unit* target = nullptr;
    if (!spellInfo->IsPositive())
    {
        target = AI_VALUE(Unit*, "current target");
        if (!target || !target->IsAlive() || !bot->IsWithinDistInMap(target, spellInfo->GetMaxRange(false)) ||
            !bot->IsWithinLOSInMap(target))
            return false;
    }

    return UseEquipped(bot, botAI, gloves, target);
}
