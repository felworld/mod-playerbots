/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ClassServiceActions.h"

#include <algorithm>
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
constexpr uint32 CONJURE_RETRIES = 6;
constexpr uint32 PORTAL_CLICK_RETRIES = 5;

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

    if (requester->GetMapId() != bot->GetMapId() || !bot->IsWithinDistInMap(requester, INTERACTION_DISTANCE * 2))
    {
        botAI->TellError("Come closer so I can hand it to you");
        return false;
    }

    uint32 const spellId = FindConjureSpell(drink);
    if (GiveConjured(requester, drink, spellId && IsRefreshmentSpell(spellId)))
        return true;

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

    uint32 const retries = retryToken.empty() ? CONJURE_RETRIES : atoi(retryToken.c_str());
    if (bot->IsNonMeleeSpellCast(true))
        return Requeue(requester, kind, retries);

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

bool ConjureItemAction::GiveConjured(Player* requester, bool drink, bool includeOtherCategory)
{
    std::vector<Item*> items = parseItems(drink ? "conjured water" : "conjured food", ITERATE_ITEMS_IN_BAGS);

    // Conjure Refreshment makes a single item that is both food and drink but carries only
    // one spell category - accept the other category when that is all the mage can make.
    if (items.empty() && includeOtherCategory)
        items = parseItems(drink ? "conjured food" : "conjured water", ITERATE_ITEMS_IN_BAGS);

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
    cmd << sPlayerbotAIConfig.commandPrefix << "conjure " << kind << " " << (retriesLeft - 1);
    botAI->HandleCommand(CHAT_MSG_WHISPER, cmd.str(), requester);
    botAI->SetNextCheckDelay(1000);
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
    std::vector<Player*> helpers;
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

        helpers.push_back(member);
    }

    if (helpers.size() < 2)
    {
        botAI->TellError("I need two more group members beside me to perform the ritual");
        return false;
    }

    if (!bot->HasItemCount(ITEM_SOUL_SHARD, 1))
        bot->AddItem(ITEM_SOUL_SHARD, 1);

    if (!botAI->CanCastSpell(SPELL_RITUAL_OF_SUMMONING, bot, true) ||
        !botAI->CastSpell(SPELL_RITUAL_OF_SUMMONING, bot))
    {
        botAI->TellError("I cannot begin the ritual right now");
        return false;
    }

    // The finishing spell summons whoever the caster has selected when the last participant
    // clicks the portal (CastSpell restored the previous selection).
    bot->SetSelection(requester->GetGUID());

    uint32 recruited = 0;
    for (Player* helper : helpers)
    {
        std::ostringstream cmd;
        cmd << sPlayerbotAIConfig.commandPrefix << "use summoning portal " << PORTAL_CLICK_RETRIES;
        GET_PLAYERBOT_AI(helper)->HandleCommand(CHAT_MSG_WHISPER, cmd.str(), requester);
        if (++recruited >= 3)
            break;
    }

    botAI->TellMasterNoFacing("We are casting a summoning ritual - accept when the portal opens");
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
        cmd << sPlayerbotAIConfig.commandPrefix << "use summoning portal " << (retries - 1);
        botAI->HandleCommand(CHAT_MSG_WHISPER, cmd.str(), requester);
        botAI->SetNextCheckDelay(1000);
        return true;
    }

    WorldPacket data(CMSG_GAMEOBJ_USE, 8);
    data << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data);
    return true;
}
