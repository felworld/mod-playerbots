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
#include "ObjectAccessor.h"
#include "ObjectDefines.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TradeOfferMgr.h"
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
// How long a summoning requester gets to accept the bot's group invite (polled every 2s).
constexpr uint32 RITUAL_INVITE_RETRIES = 30;
constexpr uint32 RITUAL_INVITE_POLL_SECS = 2;

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

// Summoning requesters are often strangers to an ungrouped random bot, so TellError
// (master-only) would be lost - whisper them directly instead.
void WhisperTo(Player* bot, Player* requester, std::string const& text)
{
    if (bot->IsInWorld() && requester->IsInWorld())
        bot->Whisper(text, LANG_UNIVERSAL, requester);
}

bool GroupHasRealPlayers(Group* group)
{
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member)
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || memberAI->IsRealPlayer())
            return true;
    }

    return false;
}

// The bot's own circle rides free: its master, groupmates, and guildmates.
// A real player outside it is a paying customer for portals and summons.
bool IsServiceCircle(PlayerbotAI* botAI, Player* requester)
{
    Player* bot = botAI->GetBot();
    if (requester == botAI->GetMaster())
        return true;

    Group* group = bot->GetGroup();
    if (group && group->IsMember(requester->GetGUID()) && !group->isBGGroup() && !group->isBFGroup())
        return true;

    return bot->GetGuildId() && bot->GetGuildId() == requester->GetGuildId();
}

bool IsRealCustomer(Player* requester)
{
    PlayerbotAI* requesterAI = GET_PLAYERBOT_AI(requester);
    return !requesterAI || requesterAI->IsRealPlayer();
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
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    if (!requester)
        return false;

    // The command is unsecured, so the asker may be a stranger TellError
    // would never reach - answer whoever actually asked.
    auto tell = [this, requester](std::string const& text)
    {
        if (requester == GetMaster())
            botAI->TellError(text);
        else
            WhisperTo(bot, requester, text);
    };

    if (bot->getClass() != CLASS_MAGE)
    {
        tell("I am not a mage - I cannot open portals");
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
        tell("I have not learned any portal spells");
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
        if (requester == GetMaster())
            botAI->TellMasterNoFacing(out.str());
        else
            WhisperTo(bot, requester, out.str());
        return false;
    }

    // A real player outside the bot's circle buys the portal: the deal
    // collects the tip through a trade window - traveling over first when
    // they are in another city - and the cast follows the payment.
    if (IsRealCustomer(requester) && requester != bot && !IsServiceCircle(botAI, requester))
    {
        if (!sRandomPlayerbotMgr.IsRandomBot(bot) || !sPlayerbotAIConfig.classServicePortalTip)
        {
            WhisperTo(bot, requester, "I only open portals for my own group and guild");
            return false;
        }

        if (sTradeOfferMgr->HasDealWith(bot->GetGUID(), requester->GetGUID()))
            return true;  // already arranged; fulfillment is on it

        std::string error;
        if (!MarketQuote::CommitService(botAI, requester, TradeService::Portal, spellId, city, error))
        {
            WhisperTo(bot, requester, "No portal - " + error + ".");
            return false;
        }
        return true;
    }

    if (!bot->HasItemCount(ITEM_RUNE_OF_PORTALS, 1))
        bot->AddItem(ITEM_RUNE_OF_PORTALS, 1);

    if (!botAI->CanCastSpell(spellId, bot, true) || !botAI->CastSpell(spellId, bot))
    {
        tell("I cannot open that portal right now");
        return false;
    }

    if (requester == GetMaster())
        botAI->TellMasterNoFacing("Opening a portal to " + city + " - step through before it fades");
    else
        WhisperTo(bot, requester, "Opening a portal to " + city + " - step through before it fades");
    return true;
}

bool RitualOfSummoningAction::Execute(Event event)
{
    // Requeued invite-wait calls carry "wait <retries> <guid counter>" and are queued
    // with the bot itself as owner, so the requester logging out mid-wait cannot leave
    // a dangling pointer in the command queue.
    std::istringstream params(event.getParam());
    std::string token;
    params >> token;

    bool const waiting = token == "wait";
    uint32 retries = 0;
    Player* requester = nullptr;
    if (waiting)
    {
        uint32 counter = 0;
        params >> retries >> counter;
        requester = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(counter));
    }
    else
        requester = event.getOwner() ? event.getOwner() : GetMaster();

    if (!requester)
        return false;

    if (bot->getClass() != CLASS_WARLOCK)
    {
        WhisperTo(bot, requester, "I am not a warlock - I cannot summon you");
        return false;
    }

    if (!bot->HasSpell(SPELL_RITUAL_OF_SUMMONING))
    {
        WhisperTo(bot, requester, "I have not learned Ritual of Summoning");
        return false;
    }

    // A stranger's summon is a paid service: quote and register the deal
    // before the ritual machinery starts. The tip is collected by trade once
    // they land (they are far away by definition, so it cannot come first).
    // Requeued polls skip past this via HasDealWith, and once the requester
    // accepts the group invite they count as circle.
    if (IsRealCustomer(requester) && requester != bot && !IsServiceCircle(botAI, requester) &&
        !sTradeOfferMgr->HasDealWith(bot->GetGUID(), requester->GetGUID()))
    {
        if (!sRandomPlayerbotMgr.IsRandomBot(bot) || !sPlayerbotAIConfig.classServiceSummonTip)
        {
            WhisperTo(bot, requester, "I only run summoning rituals for my own group and guild");
            return false;
        }

        std::string error;
        if (!MarketQuote::CommitService(botAI, requester, TradeService::Summon,
                SPELL_RITUAL_OF_SUMMONING, "", error))
        {
            WhisperTo(bot, requester, "No summon - " + error + ".");
            return false;
        }
    }

    Group* group = bot->GetGroup();
    if (group && group->IsMember(requester->GetGUID()))
        return PerformRitual(requester);

    return waiting ? WaitForAccept(requester, retries) : InviteRequester(requester);
}

// The requester is not in the bot's group: form one around the bot instead of making
// them invite the bot - a random bot teleports to its inviter when it accepts, which
// defeats the summon. The bot stays put, invites the requester, and begins the ritual
// once they accept.
bool RitualOfSummoningAction::InviteRequester(Player* requester)
{
    if (Group* pending = requester->GetGroupInvite())
    {
        if (pending->GetLeaderGUID() == bot->GetGUID())
            return WaitForAccept(requester, RITUAL_INVITE_RETRIES);

        WhisperTo(bot, requester, "You already have a group invite pending - settle that one and ask me again");
        return false;
    }

    if (requester->GetGroup())
    {
        WhisperTo(bot, requester,
                  "You are already in a group - I can only summon members of my own group. Leave yours and ask me again");
        return false;
    }

    Group* group = bot->GetGroup();
    if (group && !group->IsLeader(bot->GetGUID()))
    {
        if (GroupHasRealPlayers(group))
        {
            WhisperTo(bot, requester, "I am already traveling with a group");
            return false;
        }

        // A bot-only group is not worth keeping the requester waiting for.
        bot->RemoveFromGroup();
        group = nullptr;
    }

    if (group && group->IsFull())
    {
        WhisperTo(bot, requester, "My group is full - I cannot invite you");
        return false;
    }

    // Clear any invite aimed at the bot so the leader invite below can be created.
    if (Group* botPending = bot->GetGroupInvite(); botPending && botPending->GetLeaderGUID() != bot->GetGUID())
        bot->UninviteFromGroup();

    WorldPacket p;
    p << requester->GetName();
    p << uint32(0);  // roles mask
    bot->GetSession()->HandleGroupInviteOpcode(p);

    Group* pending = requester->GetGroupInvite();
    if (!pending || pending->GetLeaderGUID() != bot->GetGUID())
    {
        WhisperTo(bot, requester, "I could not send you a group invite");
        return false;
    }

    WhisperTo(bot, requester, "Accept my group invite and I will begin the summoning ritual");
    return WaitForAccept(requester, RITUAL_INVITE_RETRIES);
}

bool RitualOfSummoningAction::WaitForAccept(Player* requester, uint32 retriesLeft)
{
    Group* pending = requester->GetGroupInvite();
    if (!pending || pending->GetLeaderGUID() != bot->GetGUID())
        return false;  // declined - an accept lands in PerformRitual on the next poll

    if (!retriesLeft)
    {
        WhisperTo(bot, requester, "I will stop waiting - ask me again when you are ready");
        return false;
    }

    std::ostringstream cmd;
    cmd << "ritual wait " << (retriesLeft - 1) << " " << requester->GetGUID().GetCounter();
    botAI->QueueChatCommand(cmd.str(), bot, CHAT_MSG_WHISPER, RITUAL_INVITE_POLL_SECS);
    return true;
}

bool RitualOfSummoningAction::PerformRitual(Player* requester)
{
    Group* group = bot->GetGroup();

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
        WhisperTo(bot, requester, "I need two more party members with me, and there is no one nearby to recruit");
        return false;
    }

    if (!recruits.empty() && !group->isRaidGroup() &&
        group->GetMembersCount() + recruits.size() > MAXGROUPSIZE)
    {
        WhisperTo(bot, requester, "Our group does not have room for the helpers I would recruit");
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
        WhisperTo(bot, requester, "I could not recruit enough helpers for the ritual");
        return false;
    }

    if (!bot->HasItemCount(ITEM_SOUL_SHARD, 1))
        bot->AddItem(ITEM_SOUL_SHARD, 1);

    if (!botAI->CanCastSpell(SPELL_RITUAL_OF_SUMMONING, bot, true) ||
        !botAI->CastSpell(SPELL_RITUAL_OF_SUMMONING, bot))
    {
        undoRecruiting();
        WhisperTo(bot, requester, "I cannot begin the ritual right now");
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
        WhisperTo(bot, requester, "The ritual fizzled");
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

    WhisperTo(bot, requester, "We are casting a summoning ritual - accept when the portal opens");
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
