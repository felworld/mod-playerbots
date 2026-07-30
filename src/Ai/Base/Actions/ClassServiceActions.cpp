/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ClassServiceActions.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "ChatHelper.h"
#include "Event.h"
#include "GameObject.h"
#include "Group.h"
#include "ObjectDefines.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Util.h"
#include "WorldPacket.h"

namespace
{
constexpr uint32 SPELL_RITUAL_OF_SUMMONING = 698;
constexpr uint32 ITEM_SOUL_SHARD = 6265;
constexpr uint32 ITEM_RUNE_OF_PORTALS = 17032;
// Generous enough for a conjure cast plus walking across handover range (1s per retry).
constexpr uint32 CONJURE_RETRIES = 20;
constexpr uint32 PORTAL_CLICK_RETRIES = 5;
constexpr uint32 RITUAL_CLICKERS_NEEDED = 2;
// Clickers must keep channeling the ritual visual until the portal finishes (about 5s),
// or GameObject::CheckRitualList prunes them - hold their AI still slightly longer.
constexpr uint32 RITUAL_CHANNEL_FREEZE_MS = 8000;
constexpr uint32 RITUAL_DEPART_DELAY_SECS = 60;

std::string ToLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
    return text;
}

bool SpellNameMatches(SpellInfo const* spellInfo, std::string const& loweredName)
{
    std::wstring wname;
    if (!Utf8toWStr(spellInfo->SpellName[0], wname))
        return false;

    wstrToLower(wname);
    return Utf8FitTo(loweredName, wname);
}

bool IsRefreshmentSpell(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    return spellInfo && SpellNameMatches(spellInfo, "conjure refreshment");
}
}

bool ConjureItemAction::Execute(Event event)
{
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    if (!requester)
        return false;

    std::istringstream params(event.getParam());
    std::string kind;
    std::string retryToken;
    params >> kind >> retryToken;

    bool const drink = kind == "water" || kind == "drink";
    if (!drink && kind != "food")
    {
        botAI->TellError("Ask me to 'conjure food' or 'conjure water'");
        return false;
    }

    if (requester->GetMapId() != bot->GetMapId() ||
        !bot->IsWithinDistInMap(requester, sPlayerbotAIConfig.sightDistance))
    {
        botAI->TellError("You are too far away for me to bring it to you");
        return false;
    }

    uint32 const retries = retryToken.empty() ? CONJURE_RETRIES : atoi(retryToken.c_str());
    uint32 const spellId = FindConjureSpell(drink);

    std::vector<Item*> items = CollectConjured(drink, spellId && IsRefreshmentSpell(spellId));
    if (!items.empty())
    {
        // Walk into handover range instead of making the requester come to us.
        if (!bot->IsWithinDistInMap(requester, INTERACTION_DISTANCE * 2))
        {
            bot->GetMotionMaster()->MovePoint(0, requester->GetPositionX(), requester->GetPositionY(),
                                              requester->GetPositionZ());
            return Requeue(requester, kind, retries);
        }

        return GiveConjured(requester, items);
    }

    if (bot->getClass() != CLASS_MAGE)
    {
        botAI->TellError("I am not a mage - I cannot conjure anything");
        return false;
    }

    if (!spellId)
    {
        botAI->TellError(drink ? "I have not learned to conjure water" : "I have not learned to conjure food");
        return false;
    }

    if (bot->IsNonMeleeSpellCast(true))
        return Requeue(requester, kind, retries);

    if (bot->isMoving())
    {
        // Conjuring has a cast time - stop before casting.
        bot->GetMotionMaster()->Clear();
        bot->StopMoving();
        return Requeue(requester, kind, retries);
    }

    if (!botAI->CanCastSpell(spellId, bot, true) || !botAI->CastSpell(spellId, bot))
    {
        botAI->TellError("I cannot conjure right now");
        return false;
    }

    return Requeue(requester, kind, retries);
}

uint32 ConjureItemAction::FindConjureSpell(bool drink)
{
    std::string const wanted = drink ? "conjure water" : "conjure food";

    uint32 bestId = 0;
    uint32 bestRefreshmentId = 0;
    for (auto const& [spellId, playerSpell] : bot->GetSpellMap())
    {
        if (playerSpell->State == PLAYERSPELL_REMOVED || !playerSpell->Active)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || spellInfo->Effects[0].Effect != SPELL_EFFECT_CREATE_ITEM)
            continue;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(spellInfo->Effects[0].ItemType);
        if (!proto || bot->CanUseItem(proto) != EQUIP_ERR_OK)
            continue;

        if (SpellNameMatches(spellInfo, wanted) && spellId > bestId)
            bestId = spellId;
        else if (SpellNameMatches(spellInfo, "conjure refreshment") && spellId > bestRefreshmentId)
            bestRefreshmentId = spellId;
    }

    return bestId ? bestId : bestRefreshmentId;
}

std::vector<Item*> ConjureItemAction::CollectConjured(bool drink, bool includeOtherCategory)
{
    std::vector<Item*> items = parseItems(drink ? "conjured water" : "conjured food", ITERATE_ITEMS_IN_BAGS);

    // Conjure Refreshment makes a single item that is both food and drink but carries only
    // one spell category - accept the other category when that is all the mage can make.
    if (items.empty() && includeOtherCategory)
        items = parseItems(drink ? "conjured food" : "conjured water", ITERATE_ITEMS_IN_BAGS);

    return items;
}

bool ConjureItemAction::GiveConjured(Player* requester, std::vector<Item*> const& items)
{
    bool given = false;
    for (Item* item : items)
    {
        if (requester->CanUseItem(item->GetTemplate()) != EQUIP_ERR_OK)
            continue;

        ItemPosCountVec dest;
        if (requester->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false) != EQUIP_ERR_OK)
        {
            botAI->TellError("Your bags are full");
            break;
        }

        std::ostringstream out;
        out << "Here you go - " << chat->FormatItem(item->GetTemplate(), item->GetCount());

        bot->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
        item->SetOwnerGUID(requester->GetGUID());
        requester->MoveItemToInventory(dest, item, true);

        botAI->TellMasterNoFacing(out.str());
        given = true;
    }

    return given;
}

bool ConjureItemAction::Requeue(Player* requester, std::string const& kind, uint32 retriesLeft)
{
    if (!retriesLeft)
    {
        botAI->TellError("I could not conjure that");
        return false;
    }

    std::ostringstream cmd;
    cmd << "conjure " << kind << " " << (retriesLeft - 1);
    botAI->QueueChatCommand(cmd.str(), requester, CHAT_MSG_WHISPER, 1);
    return true;
}

bool OpenPortalAction::Execute(Event event)
{
    if (bot->getClass() != CLASS_MAGE)
    {
        botAI->TellError("I am not a mage - I cannot open portals");
        return false;
    }

    std::string const dest = ToLower(event.getParam());

    std::vector<std::pair<uint32, std::string>> portals;  // spell id, city name
    for (auto const& [spellId, playerSpell] : bot->GetSpellMap())
    {
        if (playerSpell->State == PLAYERSPELL_REMOVED || !playerSpell->Active)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        std::string const name = spellInfo->SpellName[0];
        if (ToLower(name).rfind("portal: ", 0) != 0)
            continue;

        portals.emplace_back(spellId, name.substr(8));
    }

    if (portals.empty())
    {
        botAI->TellError("I have not learned any portal spells");
        return false;
    }

    uint32 spellId = 0;
    std::string city;
    if (!dest.empty())
    {
        for (auto const& [portalSpellId, portalCity] : portals)
        {
            if (ToLower(portalCity).find(dest) != std::string::npos)
            {
                spellId = portalSpellId;
                city = portalCity;
                break;
            }
        }
    }

    if (!spellId)
    {
        std::ostringstream out;
        out << "I can open portals to: ";
        for (size_t i = 0; i < portals.size(); ++i)
        {
            if (i)
                out << ", ";
            out << portals[i].second;
        }
        botAI->TellMasterNoFacing(out.str());
        return false;
    }

    if (!bot->HasItemCount(ITEM_RUNE_OF_PORTALS, 1))
        bot->AddItem(ITEM_RUNE_OF_PORTALS, 1);

    if (!botAI->CanCastSpell(spellId, bot, true) || !botAI->CastSpell(spellId, bot))
    {
        botAI->TellError("I cannot open that portal right now");
        return false;
    }

    botAI->TellMasterNoFacing("Opening a portal to " + city + " - step through before it fades");
    return true;
}

bool RitualOfSummoningAction::Execute(Event event)
{
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    if (!requester)
        return false;

    if (bot->getClass() != CLASS_WARLOCK)
    {
        botAI->TellError("I am not a warlock - I cannot summon you");
        return false;
    }

    if (!bot->HasSpell(SPELL_RITUAL_OF_SUMMONING))
    {
        botAI->TellError("I have not learned Ritual of Summoning");
        return false;
    }

    Group* group = bot->GetGroup();
    if (!group || !group->IsMember(requester->GetGUID()))
    {
        botAI->TellError("We must be in the same group");
        return false;
    }

    // The ritual portal needs two participants besides the caster.
    std::vector<Player*> clickers;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || member == requester || !member->IsAlive())
            continue;

        if (member->GetMapId() != bot->GetMapId() ||
            !bot->IsWithinDistInMap(member, sPlayerbotAIConfig.sightDistance))
            continue;

        if (!GET_PLAYERBOT_AI(member))
            continue;

        clickers.push_back(member);
        if (clickers.size() >= RITUAL_CLICKERS_NEEDED)
            break;
    }

    // Not enough groupmates around: recruit bystander bots into the group for the ritual,
    // like asking strangers at a summoning stone. They leave the group again a minute later.
    std::vector<Player*> recruits;
    if (clickers.size() < RITUAL_CLICKERS_NEEDED)
    {
        GuidVector nearby = AI_VALUE(GuidVector, "nearest friendly players");
        for (ObjectGuid const guid : nearby)
        {
            if (clickers.size() + recruits.size() >= RITUAL_CLICKERS_NEEDED)
                break;

            Unit* unit = botAI->GetUnit(guid);
            Player* candidate = unit ? unit->ToPlayer() : nullptr;
            if (!candidate || !candidate->IsAlive() || candidate->IsInCombat() || candidate->GetGroup())
                continue;

            if (candidate->GetTeamId() != bot->GetTeamId())
                continue;

            if (!GET_PLAYERBOT_AI(candidate) || !sRandomPlayerbotMgr.IsRandomBot(candidate))
                continue;

            recruits.push_back(candidate);
        }
    }

    if (clickers.size() + recruits.size() < RITUAL_CLICKERS_NEEDED)
    {
        botAI->TellError("I need two more party members with me, and there is no one nearby to recruit");
        return false;
    }

    if (!recruits.empty() && !group->isRaidGroup() &&
        group->GetMembersCount() + recruits.size() > MAXGROUPSIZE)
    {
        botAI->TellError("Our group does not have room for the helpers I would recruit");
        return false;
    }

    std::vector<Player*> added;
    for (Player* recruit : recruits)
    {
        if (group->IsFull() || !group->AddMember(recruit))
            break;

        added.push_back(recruit);
        clickers.push_back(recruit);
    }

    auto undoRecruiting = [&added]()
    {
        for (Player* recruit : added)
            recruit->RemoveFromGroup();
    };

    if (clickers.size() < RITUAL_CLICKERS_NEEDED)
    {
        undoRecruiting();
        botAI->TellError("I could not recruit enough helpers for the ritual");
        return false;
    }

    if (!bot->HasItemCount(ITEM_SOUL_SHARD, 1))
        bot->AddItem(ITEM_SOUL_SHARD, 1);

    if (!botAI->CanCastSpell(SPELL_RITUAL_OF_SUMMONING, bot, true) ||
        !botAI->CastSpell(SPELL_RITUAL_OF_SUMMONING, bot))
    {
        undoRecruiting();
        botAI->TellError("I cannot begin the ritual right now");
        return false;
    }

    // The finishing spell summons whoever the caster has selected when the last participant
    // clicks the portal (CastSpell restored the previous selection).
    bot->SetSelection(requester->GetGUID());

    // The channeled cast spawns the portal synchronously.
    GameObject* portal = bot->GetGameObject(SPELL_RITUAL_OF_SUMMONING);
    if (!portal)
    {
        bot->InterruptNonMeleeSpells(true);
        undoRecruiting();
        botAI->TellError("The ritual fizzled");
        return false;
    }

    for (uint32 i = 0; i < RITUAL_CLICKERS_NEEDED; ++i)
    {
        Player* clicker = clickers[i];
        clicker->GetMotionMaster()->Clear();
        clicker->StopMoving();
        portal->Use(clicker);
        GET_PLAYERBOT_AI(clicker)->SetNextCheckDelay(RITUAL_CHANNEL_FREEZE_MS);
    }

    for (Player* recruit : added)
        GET_PLAYERBOT_AI(recruit)->QueueChatCommand("ritual depart", recruit, CHAT_MSG_WHISPER,
                                                    RITUAL_DEPART_DELAY_SECS);

    botAI->TellMasterNoFacing("We are casting a summoning ritual - accept when the portal opens");
    botAI->SetNextCheckDelay(RITUAL_CHANNEL_FREEZE_MS);
    return true;
}

bool RitualDepartAction::Execute(Event /*event*/)
{
    if (!bot->GetGroup() || !sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    bot->RemoveFromGroup();
    return true;
}

bool UseSummoningPortalAction::Execute(Event event)
{
    std::string const param = event.getParam();
    uint32 const retries = param.empty() ? PORTAL_CLICK_RETRIES : atoi(param.c_str());

    GameObject* portal = nullptr;
    GuidVector gos = AI_VALUE(GuidVector, "nearest game objects no los");
    for (ObjectGuid const guid : gos)
    {
        GameObject* go = botAI->GetGameObject(guid);
        if (!go || !go->isSpawned() || go->GetGoType() != GAMEOBJECT_TYPE_SUMMONING_RITUAL)
            continue;

        Unit* owner = go->GetOwner();
        if (!owner || !owner->IsPlayer() || !bot->IsInSameRaidWith(owner->ToPlayer()))
            continue;

        portal = go;
        break;
    }

    if (!portal)
    {
        Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
        if (!retries || !requester)
            return false;

        std::ostringstream cmd;
        cmd << "use summoning portal " << (retries - 1);
        botAI->QueueChatCommand(cmd.str(), requester, CHAT_MSG_WHISPER, 1);
        return true;
    }

    WorldPacket data(CMSG_GAMEOBJ_USE, 8);
    data << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data);
    return true;
}
