/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EngineeringDeviceActions.h"

#include <mutex>

#include "ChatHelper.h"
#include "Event.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace
{
    std::vector<std::function<void(JumperCablesNotification const&)>>& CablesListeners()
    {
        static std::vector<std::function<void(JumperCablesNotification const&)>> listeners;
        return listeners;
    }

    std::mutex& CablesListenersMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    uint32 OnUseSpellId(ItemTemplate const* proto)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            if (proto->Spells[i].SpellId > 0 && proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
                return proto->Spells[i].SpellId;

        return 0;
    }

    // Fire the item's on-use spell; unitTarget only for spells cast on someone else.
    bool UseDevice(Player* bot, PlayerbotAI* botAI, Item* item, Unit* unitTarget = nullptr)
    {
        uint32 spellId = OnUseSpellId(item->GetTemplate());
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return false;

        // Deploying a device has a setup cast that moving would interrupt.
        if (bot->isMoving())
        {
            bot->StopMoving();
            botAI->SetNextCheckDelay(sPlayerbotAIConfig.reactDelay);
            return false;
        }

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

        if (uint32 castTime = spellInfo->CalcCastTime())
            botAI->SetNextCheckDelay(castTime + sPlayerbotAIConfig.reactDelay);

        return true;
    }
}

void RegisterJumperCablesListener(std::function<void(JumperCablesNotification const&)> listener)
{
    std::lock_guard<std::mutex> lock(CablesListenersMutex());
    CablesListeners().push_back(std::move(listener));
}

void FireJumperCablesNotification(JumperCablesNotification const& notification)
{
    std::lock_guard<std::mutex> lock(CablesListenersMutex());
    for (auto const& listener : CablesListeners())
        listener(notification);
}

namespace EngineeringDevices
{
    std::vector<Tier> const& TargetDummies()
    {
        static std::vector<Tier> const ladder = {
            { 4366, 85 },    // Target Dummy
            { 4392, 185 },   // Advanced Target Dummy
            { 16023, 275 },  // Masterwork Target Dummy
        };
        return ladder;
    }

    std::vector<Tier> const& JumperCables()
    {
        static std::vector<Tier> const ladder = {
            { 7148, 165 },   // Goblin Jumper Cables
            { 18587, 265 },  // Goblin Jumper Cables XL
        };
        return ladder;
    }

    std::vector<Tier> const& ExplosiveSheep()
    {
        static std::vector<Tier> const ladder = {
            { 4384, 150 },  // Explosive Sheep
        };
        return ladder;
    }

    uint32 BestForSkill(std::vector<Tier> const& ladder, uint32 skill)
    {
        uint32 best = 0;
        for (Tier const& tier : ladder)
        {
            if (tier.rank > skill)
                break;

            best = tier.itemId;
        }

        return best;
    }

    Item* FindBestCarried(Player* bot, std::vector<Tier> const& ladder)
    {
        for (auto itr = ladder.rbegin(); itr != ladder.rend(); ++itr)
        {
            Item* item = bot->GetItemByEntry(itr->itemId);
            if (!item)
                continue;

            if (bot->CanUseItem(item) != EQUIP_ERR_OK || bot->HasSpellCooldown(OnUseSpellId(item->GetTemplate())))
                continue;

            return item;
        }

        return nullptr;
    }

    bool TargetDummyWouldHelp(Player* bot)
    {
        // Inside an instance the dummy's untargeted taunt is as likely to rip a pull off
        // the tank as it is to save the bot. A group could use one cleverly; a bot can't,
        // so it just doesn't.
        if (bot->GetMap()->IsDungeon())
            return false;

        for (Unit* attacker : bot->getAttackers())
        {
            // CanHaveThreatList() is exactly what the taunt aura handler checks: players,
            // pets, totems, triggers and player-summoned guardians have no threat list, so
            // the dummy's taunt is a no-op on them.
            if (attacker && attacker->CanHaveThreatList())
                return true;
        }

        return false;
    }
}

bool UseTargetDummyAction::isUseful()
{
    return bot->IsInCombat() && EngineeringDevices::TargetDummyWouldHelp(bot) &&
           botAI->DuelAllowsConsumable(DuelConsumables::BANDAGES);
}

bool UseTargetDummyAction::isPossible()
{
    return EngineeringDevices::FindBestCarried(bot, EngineeringDevices::TargetDummies()) != nullptr;
}

bool UseTargetDummyAction::Execute(Event /*event*/)
{
    Item* item = EngineeringDevices::FindBestCarried(bot, EngineeringDevices::TargetDummies());
    return item && UseDevice(bot, botAI, item);
}

bool UseExplosiveSheepAction::isUseful()
{
    return bot->IsInCombat() && botAI->DuelAllowsConsumable(DuelConsumables::BANDAGES);
}

bool UseExplosiveSheepAction::isPossible()
{
    return EngineeringDevices::FindBestCarried(bot, EngineeringDevices::ExplosiveSheep()) != nullptr;
}

bool UseExplosiveSheepAction::Execute(Event /*event*/)
{
    Item* item = EngineeringDevices::FindBestCarried(bot, EngineeringDevices::ExplosiveSheep());
    return item && UseDevice(bot, botAI, item);
}

bool UseJumperCablesAction::isUseful()
{
    if (bot->IsInCombat())
        return false;

    // Classes with a real resurrection spell don't need to improvise.
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
        case CLASS_PALADIN:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
            return false;
        default:
            break;
    }

    Unit* target = AI_VALUE(Unit*, "party member to jumper cable");
    return target && bot->IsWithinDistInMap(target, INTERACTION_DISTANCE);
}

bool UseJumperCablesAction::isPossible()
{
    return EngineeringDevices::FindBestCarried(bot, EngineeringDevices::JumperCables()) != nullptr;
}

bool UseJumperCablesAction::Execute(Event /*event*/)
{
    Unit* target = AI_VALUE(Unit*, "party member to jumper cable");
    if (!target)
        return false;

    Item* item = EngineeringDevices::FindBestCarried(bot, EngineeringDevices::JumperCables());
    if (!item || !UseDevice(bot, botAI, item, target))
        return false;

    JumperCablesNotification notification;
    notification.user = bot;
    notification.itemName = item->GetTemplate()->Name1;
    notification.targetName = target->GetName();
    FireJumperCablesNotification(notification);

    // The one gadget worth a chat line: out of combat, aimed at a person, and
    // it explains why the bot is standing over a corpse channeling.
    if (sPlayerbotAIConfig.engineeringChatter)
        botAI->TellMasterNoFacing(PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "use_jumper_cables",
            "Trying %item on %target",
            {{"%item", chat->FormatItem(item->GetTemplate())}, {"%target", target->GetName()}}));
    return true;
}
