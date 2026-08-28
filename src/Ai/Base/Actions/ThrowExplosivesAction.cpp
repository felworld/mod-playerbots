/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ThrowExplosivesAction.h"

#include "Bag.h"
#include "ChatHelper.h"
#include "Event.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "Random.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace
{
    uint32 OnUseSpellId(ItemTemplate const* proto)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            if (proto->Spells[i].SpellId > 0 && proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
                return proto->Spells[i].SpellId;

        return 0;
    }

    SpellInfo const* ExplosiveSpell(ItemTemplate const* proto)
    {
        if (!proto || proto->Class != ITEM_CLASS_TRADE_GOODS || proto->SubClass != ITEM_SUBCLASS_EXPLOSIVES)
            return nullptr;

        uint32 spellId = OnUseSpellId(proto);
        return spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    }

    bool HasSchoolDamage(SpellInfo const* spellInfo)
    {
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (spellInfo->Effects[i].Effect == SPELL_EFFECT_SCHOOL_DAMAGE)
                return true;

        return false;
    }

    // Best = highest engineering rank, ties broken by item level; skips items the bot
    // cannot use (skill/level) and items whose shared explosives cooldown is running.
    Item* FindBestUsable(Player* bot, std::function<bool(ItemTemplate const*)> const& matches)
    {
        Item* best = nullptr;
        ThrowExplosivesAction::VisitExplosives(bot, [bot, &best, &matches](Item* item)
        {
            ItemTemplate const* proto = item->GetTemplate();
            if (!matches(proto))
                return;

            if (bot->CanUseItem(item) != EQUIP_ERR_OK || bot->HasSpellCooldown(OnUseSpellId(proto)))
                return;

            if (!best || proto->RequiredSkillRank > best->GetTemplate()->RequiredSkillRank ||
                (proto->RequiredSkillRank == best->GetTemplate()->RequiredSkillRank &&
                 proto->ItemLevel > best->GetTemplate()->ItemLevel))
                best = item;
        });

        return best;
    }
}

bool ThrowExplosivesAction::IsThrownExplosive(ItemTemplate const* proto)
{
    SpellInfo const* spellInfo = ExplosiveSpell(proto);
    return spellInfo && (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION) && HasSchoolDamage(spellInfo);
}

bool ThrowExplosivesAction::IsStunExplosive(ItemTemplate const* proto)
{
    if (!IsThrownExplosive(proto))
        return false;

    SpellInfo const* spellInfo = ExplosiveSpell(proto);
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_APPLY_AURA &&
            spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_STUN)
            return true;

    return false;
}

bool ThrowExplosivesAction::IsSapperCharge(ItemTemplate const* proto)
{
    SpellInfo const* spellInfo = ExplosiveSpell(proto);
    if (!spellInfo || (spellInfo->Targets & TARGET_FLAG_DEST_LOCATION) || !HasSchoolDamage(spellInfo))
        return false;

    // Sappers are the caster-centered blasts that hurt their user too.
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_SCHOOL_DAMAGE &&
            spellInfo->Effects[i].TargetA.GetTarget() == TARGET_UNIT_CASTER)
            return true;

    return false;
}

void ThrowExplosivesAction::VisitExplosives(Player* bot, std::function<void(Item*)> const& visit)
{
    auto isExplosive = [](ItemTemplate const* proto)
    {
        return proto && proto->Class == ITEM_CLASS_TRADE_GOODS && proto->SubClass == ITEM_SUBCLASS_EXPLOSIVES;
    };

    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (isExplosive(item->GetTemplate()))
                visit(item);

    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        if (Bag* bag = bot->GetBagByPos(bagSlot))
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                if (Item* item = bag->GetItemByPos(slot))
                    if (isExplosive(item->GetTemplate()))
                        visit(item);
}

Item* ThrowExplosivesAction::FindBestThrown(Player* bot, bool requireStun)
{
    return FindBestUsable(bot, [requireStun](ItemTemplate const* proto)
    {
        return requireStun ? IsStunExplosive(proto) : IsThrownExplosive(proto);
    });
}

Item* ThrowExplosivesAction::FindBestSapper(Player* bot)
{
    return FindBestUsable(bot, [](ItemTemplate const* proto) { return IsSapperCharge(proto); });
}

bool ThrowExplosivesAction::CanThrowAt(Player* bot, Item* item, Unit* target)
{
    if (!target || !target->IsAlive())
        return false;

    SpellInfo const* spellInfo = ExplosiveSpell(item->GetTemplate());
    if (!spellInfo)
        return false;

    float range = spellInfo->GetMaxRange(false);
    if (range <= 0.0f)
        return false;

    return bot->IsWithinDistInMap(target, range) && bot->IsWithinLOSInMap(target);
}

Item* ThrowExplosivesAction::PickExplosive() { return FindBestThrown(bot); }

Item* GrenadeInterruptAction::PickExplosive() { return FindBestThrown(bot, /*requireStun=*/true); }

bool ThrowExplosivesAction::isUseful()
{
    return AI_VALUE(Unit*, "current target") && botAI->DuelAllowsConsumable(DuelConsumables::BANDAGES);
}

bool ThrowExplosivesAction::isPossible() { return PickExplosive() != nullptr; }

bool ThrowExplosivesAction::Execute(Event /*event*/)
{
    Item* item = PickExplosive();
    if (!item)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !CanThrowAt(bot, item, target))
        return false;

    return ThrowAt(item, target);
}

bool ThrowExplosivesAction::ThrowAt(Item* item, Unit* target)
{
    // Explosives have a short fuse cast that moving would interrupt.
    if (bot->isMoving())
    {
        bot->StopMoving();
        botAI->SetNextCheckDelay(sPlayerbotAIConfig.reactDelay);
        return false;
    }

    uint32 spellId = OnUseSpellId(item->GetTemplate());
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    if (!bot->HasInArc(CAST_ANGLE_IN_FRONT, target))
        bot->SetFacingToObject(target);

    uint8 bagIndex = item->GetBagSlot();
    uint8 slot = item->GetSlot();
    uint8 castCount = 1;
    ObjectGuid itemGuid = item->GetGUID();
    uint32 glyphIndex = 0;
    uint8 castFlags = 0;

    WorldPacket packet(CMSG_USE_ITEM);
    packet << bagIndex << slot << castCount << spellId << itemGuid << glyphIndex << castFlags;
    packet << uint32(TARGET_FLAG_DEST_LOCATION);
    packet.appendPackGUID(0);
    packet << target->GetPositionX() << target->GetPositionY() << target->GetPositionZ();

    bot->GetSession()->HandleUseItemOpcode(packet);

    if (uint32 castTime = spellInfo->CalcCastTime())
        botAI->SetNextCheckDelay(castTime + sPlayerbotAIConfig.reactDelay);

    if (roll_chance_i(sPlayerbotAIConfig.engineeringChatterChance))
        botAI->TellMasterNoFacing(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "throw_explosive",
            "Throwing %item at %target",
            {{"%item", chat->FormatItem(item->GetTemplate())}, {"%target", target->GetName()}}));
    return true;
}

bool SapperChargeAction::isUseful()
{
    // The blast hurts the bot too - only worth it with a healthy buffer.
    return bot->GetHealthPct() > 60.0f && botAI->DuelAllowsConsumable(DuelConsumables::BANDAGES);
}

bool SapperChargeAction::isPossible() { return ThrowExplosivesAction::FindBestSapper(bot) != nullptr; }

bool SapperChargeAction::Execute(Event /*event*/)
{
    Item* item = ThrowExplosivesAction::FindBestSapper(bot);
    if (!item)
        return false;

    uint32 spellId = OnUseSpellId(item->GetTemplate());
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
    packet << uint32(TARGET_FLAG_NONE);

    bot->GetSession()->HandleUseItemOpcode(packet);

    if (roll_chance_i(sPlayerbotAIConfig.engineeringChatterChance))
        botAI->TellMasterNoFacing(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "sapper_charge",
            "Setting off %item",
            {{"%item", chat->FormatItem(item->GetTemplate())}}));
    return true;
}
