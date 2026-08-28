/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NewRpgAction.h"
#include "AreaDefines.h"
#include "BroadcastHelper.h"
#include "ChatHelper.h"
#include "GossipDef.h"
#include "IVMapMgr.h"
#include "NewRpgDuelSpot.h"
#include "NewRpgInfo.h"
#include "NewRpgStrategy.h"
#include "NewRpgWpvp.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "ObjectDefines.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "QuestDef.h"
#include "Random.h"
#include "RandomPlayerbotMgr.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "TradeOfferMgr.h"
#include "TravelMgr.h"
#include "WpvpDefense.h"
#include "G3D/Vector2.h"
#include <cmath>
#include <cstdlib>

void TellRpgStatusAction::WhisperStatusChange(Player* owner, std::string const& statusName)
{
    std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
        RPG_STATUS_CHANGED_KEY, RPG_STATUS_CHANGED_DEFAULT,
        {{"%status", statusName}});
    bot->Whisper(msg, LANG_UNIVERSAL, owner);
}

bool TellRpgStatusAction::Execute(Event event)
{
    Player* owner = event.getOwner();
    if (!owner)
        return false;

    std::string const text = event.getParam();
    if (text.empty())
    {
        std::string out = botAI->rpgInfo.ToString();
        bot->Whisper(out.c_str(), LANG_UNIVERSAL, owner);
        return true;
    }

    Player* master = botAI->GetMaster();
    bool isMaster = master && master->GetGUID() == owner->GetGUID();
    bool isGM = owner->GetSession() && owner->GetSession()->GetSecurity() >= SEC_GAMEMASTER;
    if (!isMaster && !isGM)
    {
        std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
            "rpg_debug_permission_error",
            "Only your master or a GM can change my rpg status.", {});
        bot->Whisper(msg, LANG_UNIVERSAL, owner);
        return false;
    }

    std::string name = text;
    uint32 questId = 0;
    static std::string const doQuestPrefix = "do quest ";
    size_t doQuestPos = text.find(doQuestPrefix);
    if (doQuestPos != std::string::npos)
    {
        name = "do quest";
        std::string idStr = text.substr(doQuestPos + doQuestPrefix.length());
        try
        {
            questId = static_cast<uint32>(std::stoul(idStr));
        }
        catch (std::exception const&)
        {
            questId = 0;
        }
    }

    NewRpgStatus status = NewRpgInfo::StatusFromString(name);
    NewRpgInfo& info = botAI->rpgInfo;

    if (status == RPG_IDLE)
    {
        info.ChangeToIdle();
        WhisperStatusChange(owner, "IDLE");
        return true;
    }
    else if (status == RPG_REST)
    {
        info.ChangeToRest();
        bot->SetStandState(UNIT_STAND_STATE_SIT);
        WhisperStatusChange(owner, "REST");
        return true;
    }
    else if (status == RPG_WANDER_RANDOM)
    {
        info.ChangeToWanderRandom();
        WhisperStatusChange(owner, "WANDER_RANDOM");
        return true;
    }
    else if (status == RPG_WANDER_NPC)
    {
        info.ChangeToWanderNpc();
        WhisperStatusChange(owner, "WANDER_NPC");
        return true;
    }
    else if (status == RPG_GO_GRIND)
    {
        WorldPosition pos = SelectRandomGrindPos(bot);
        if (pos == WorldPosition())
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_grind_pos_error", "No grind position available.", {});
            bot->Whisper(msg, LANG_UNIVERSAL, owner);
            return false;
        }
        info.ChangeToGoGrind(pos);
        WhisperStatusChange(owner, "GO_GRIND");
        return true;
    }
    else if (status == RPG_GO_CAMP)
    {
        WorldPosition pos = SelectRandomCampPos(bot);
        if (pos == WorldPosition())
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_camp_pos_error", "No camp position available.", {});
            bot->Whisper(msg, LANG_UNIVERSAL, owner);
            return false;
        }
        info.ChangeToGoCamp(pos);
        WhisperStatusChange(owner, "GO_CAMP");
        return true;
    }
    else if (status == RPG_TRAVEL_FLIGHT)
    {
        uint32 flightMasterEntry = 0;
        WorldPosition flightMasterPos;
        std::vector<uint32> path;
        if (!SelectRandomFlightTaxiNode(flightMasterEntry, flightMasterPos, path))
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_flight_path_error", "No flight path available.", {});
            bot->Whisper(msg, LANG_UNIVERSAL, owner);
            return false;
        }
        info.ChangeToTravelFlight(flightMasterEntry, flightMasterPos, std::move(path));
        WhisperStatusChange(owner, "TRAVEL_FLIGHT");
        return true;
    }
    else if (status == RPG_OUTDOOR_PVP)
    {
        info.ChangeToOutdoorPvp();
        WhisperStatusChange(owner, "OUTDOOR_PVP");
        return true;
    }
    else if (status == RPG_DO_QUEST)
    {
        if (!questId)
        {
            for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            {
                uint32 qid = bot->GetQuestSlotQuestId(slot);
                if (!qid)
                    continue;
                std::vector<POIInfo> poi;
                if (GetQuestPOIPosAndObjectiveIdx(qid, poi, true))
                {
                    questId = qid;
                    break;
                }
            }
        }
        if (!questId)
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_no_quest_error", "No quest available; use 'do quest <id>'.", {});
            bot->Whisper(msg, LANG_UNIVERSAL, owner);
            return false;
        }
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        QuestStatus questStatus = bot->GetQuestStatus(questId);
        if (!quest || (questStatus != QUEST_STATUS_INCOMPLETE && questStatus != QUEST_STATUS_COMPLETE))
        {
            std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
                "rpg_invalid_quest_error", "Invalid quest %quest_id",
                {{"%quest_id", std::to_string(questId)}});
            bot->Whisper(msg, LANG_UNIVERSAL, owner);
            return false;
        }
        info.ChangeToDoQuest(questId, quest);
        WhisperStatusChange(owner, "DO_QUEST " + std::to_string(questId));
        return true;
    }

    std::string msg = PlayerbotTextMgr::instance().GetBotTextOrDefault(
        "rpg_unknown_status_error",
        "Unknown rpg status. Options: idle, rest, wander random, wander npc, "
        "go grind, go camp, do quest [<id>], travel flight, outdoor pvp.", {});
    bot->Whisper(msg, LANG_UNIVERSAL, owner);
    return false;
}

bool StartRpgDoQuestAction::Execute(Event event)
{
    Player* owner = event.getOwner();
    if (!owner)
        return false;

    std::string const text = event.getParam();
    PlayerbotChatHandler ch(owner);
    uint32 questId = ch.extractQuestId(text);
    const Quest* quest = sObjectMgr->GetQuestTemplate(questId);
    if (quest)
    {
        botAI->rpgInfo.ChangeToDoQuest(questId, quest);
        bot->Whisper("Start to do quest " + std::to_string(questId), LANG_UNIVERSAL, owner);
        return true;
    }
    bot->Whisper("Invalid quest " + text, LANG_UNIVERSAL, owner);
    return false;
}

bool NewRpgStatusUpdateAction::Execute(Event /*event*/)
{
    NewRpgInfo& info = botAI->rpgInfo;
    NewRpgStatus status = info.GetStatus();
    switch (status)
    {
        case RPG_IDLE:
            // A committed trade deal keeps the bot close: no flights or
            // cross-zone treks while a customer is waiting (the fulfill
            // action handles any actual trip itself).
            if (sTradeOfferMgr->HasPending(bot->GetGUID()))
                return RandomChangeStatus({RPG_WANDER_NPC, RPG_REST});

            // Moonglade has no grind mobs and only walking exits (the Timbermaw
            // tunnel), so restrict the roll to statuses that keep bots pottering
            // around Nighthaven until they take the druid flight path out.
            if (bot->GetZoneId() == ZONE_MOONGLADE)
                return RandomChangeStatus({RPG_WANDER_NPC, RPG_REST, RPG_TRAVEL_FLIGHT});

            // Capitals should feel busy: a bot idling in a friendly capital
            // usually keeps pottering around the bank/AH/inn district instead
            // of rolling an activity that would take it out of the city.
            // A market anchor (a recent Trade ad or a committed WTS/WTB deal)
            // makes the dwell guaranteed - no porting out while a buyer may
            // be typing a reply.
            if (sTravelMgr.IsFriendlyCapital(bot->GetZoneId(), bot->GetTeamId()) &&
                (sTradeOfferMgr->IsAnchored(bot->GetGUID()) ||
                 roll_chance_f(sPlayerbotAIConfig.cityDwellChance * 100.0f)))
                return RandomChangeStatus({RPG_WANDER_NPC, RPG_REST});

            return RandomChangeStatus({RPG_GO_CAMP, RPG_GO_GRIND, RPG_WANDER_RANDOM, RPG_WANDER_NPC, RPG_DO_QUEST,
                                       RPG_TRAVEL_FLIGHT, RPG_REST, RPG_OUTDOOR_PVP, RPG_GO_WPVP, RPG_DUEL_SPOT,
                                       RPG_GO_MOONGLADE});

        case RPG_GO_GRIND:
        {
            auto& data = std::get<NewRpgInfo::GoGrind>(info.data);
            WorldPosition& originalPos = data.pos;
            assert(data.pos != WorldPosition());
            // GO_GRIND -> WANDER_RANDOM
            if (bot->GetExactDist(originalPos) < 10.0f)
            {
                info.ChangeToWanderRandom();
                return true;
            }
            break;
        }
        case RPG_GO_CAMP:
        {
            auto& data = std::get<NewRpgInfo::GoCamp>(info.data);
            WorldPosition& originalPos = data.pos;
            assert(data.pos != WorldPosition());
            // GO_CAMP -> WANDER_NPC
            if (bot->GetExactDist(originalPos) < 10.0f)
            {
                info.ChangeToWanderNpc();
                return true;
            }
            break;
        }
        case RPG_WANDER_RANDOM:
        {
            // WANDER_RANDOM -> IDLE
            if (info.HasStatusPersisted(statusWanderRandomDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_WANDER_NPC:
        {
            if (info.HasStatusPersisted(statusWanderNpcDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_DO_QUEST:
        {
            // DO_QUEST -> IDLE
            if (info.HasStatusPersisted(statusDoQuestDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_TRAVEL_FLIGHT:
        {
            auto& data = std::get<NewRpgInfo::TravelFlight>(info.data);
            if (data.inFlight && !bot->IsInFlight())
            {
                // flight arrival
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_GO_MOONGLADE:
        {
            if (bot->GetZoneId() == ZONE_MOONGLADE)
            {
                // Arrived: hang around Nighthaven; the restricted idle roll
                // sends the bot out via the druid flight master later.
                info.ChangeToWanderNpc();
                return true;
            }
            if (info.HasStatusPersisted(statusGoMoongladeDuration))
            {
                // Couldn't get a cast off (combat, interrupts): give up.
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_REST:
        {
            // REST -> IDLE
            if (info.HasStatusPersisted(statusRestDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_OUTDOOR_PVP:
        {
            if (info.HasStatusPersisted(statusOutDoorPvPDuration))
            {
                info.ChangeToIdle();
                return true;
            }
            break;
        }
        case RPG_GO_WPVP:
        {
            auto& data = std::get<NewRpgInfo::GoWpvp>(info.data);
            if (!sRandomPlayerbotMgr.IsWpvpExcursionEnabled())
            {
                EndWpvpExcursion(botAI, "kill switch is off");
                return true;
            }

            if (data.deathCount >= sPlayerbotAIConfig.wpvpDeathCap)
            {
                EndWpvpExcursion(botAI, "death cap reached");
                return true;
            }

            if (!data.arrivedT)
            {
                // Travel phase: either we make it to the anchor or we give up.
                if (info.HasStatusPersisted(statusGoWpvpTravelDuration))
                {
                    EndWpvpExcursion(botAI, "travel phase timed out");
                    return true;
                }

                if (data.teleported && bot->GetMapId() == data.anchorPos.GetMapId() &&
                    bot->GetExactDist(data.anchorPos) < 20.0f)
                {
                    data.arrivedT = getMSTime();
                    // Defense responses are a quick answer to a callout, not
                    // a full patrol shift.
                    data.dwellDuration = data.defend
                                             ? urand(sPlayerbotAIConfig.wpvpDefenseDwellMinutesMin,
                                                     sPlayerbotAIConfig.wpvpDefenseDwellMinutesMax) *
                                                   MINUTE * IN_MILLISECONDS
                                             : urand(sPlayerbotAIConfig.wpvpDwellMinutesMin,
                                                     sPlayerbotAIConfig.wpvpDwellMinutesMax) *
                                                   MINUTE * IN_MILLISECONDS;
                    if (data.defend)
                        data.defendLastSeenT = data.arrivedT;
                    if (data.test)
                        LOG_INFO("playerbots", "[New RPG] Bot {} (wpvp test) reached the dwell anchor; dwelling {} min",
                                 bot->GetName(), data.dwellDuration / (MINUTE * IN_MILLISECONDS));
                }
            }
            else
            {
                // Dwell phase: go home on expiry, or if something yanked the
                // bot far away (e.g. the death-count inn teleport). Expiry
                // waits out any running fight - ending the excursion mid-swing
                // would strip the wpvp strategies and skip the trip home.
                if (!bot->IsInCombat() && GetMSTimeDiffToNow(data.arrivedT) > data.dwellDuration)
                {
                    EndWpvpExcursion(botAI, "dwell time expired");
                    return true;
                }

                if (bot->GetMapId() != data.anchorPos.GetMapId() || bot->GetExactDist(data.anchorPos) > 3000.0f)
                {
                    EndWpvpExcursion(botAI, "yanked far from the dwell anchor");
                    return true;
                }

                // Defenders (and reinforcers, whose "target" is their own
                // faction-mate) go home early once that player has been gone
                // for a while. Dead-but-in-zone still counts as present: a
                // ghost jogging back to their corpse is a fight that isn't
                // over yet.
                if (data.defend && data.defendTarget)
                {
                    Player* attacker = ObjectAccessor::FindPlayer(data.defendTarget);
                    bool present = attacker && attacker->IsInWorld() && attacker->GetMapId() == bot->GetMapId() &&
                                   attacker->GetZoneId() == data.zoneId;
                    if (present)
                    {
                        data.defendLastSeenT = getMSTime();
                        // Help has arrived: a live defender sharing the zone
                        // with the attacker marks the fight handled - the
                        // board cancels any pending WorldDefense plea and
                        // resets the spree tally (screening out reinforcers,
                        // who dwell on the attacker's own side, and arrivals
                        // too far below the attacker's level to count).
                        if (bot->IsAlive())
                            WpvpDefenseBoard::instance().NoteDefenderOnScene(data.defendTarget, bot->GetTeamId(),
                                                                             bot->GetLevel());
                    }
                    else if (!bot->IsInCombat() && GetMSTimeDiffToNow(data.defendLastSeenT) > 90 * IN_MILLISECONDS)
                    {
                        EndWpvpExcursion(botAI, "defense target is gone");
                        return true;
                    }
                }
            }
            break;
        }
        case RPG_DUEL_SPOT:
        {
            auto& data = std::get<NewRpgInfo::DuelSpot>(info.data);
            if (!sPlayerbotAIConfig.enableBotDuels)
            {
                EndDuelSpotHangout(botAI, "bot duels are disabled");
                return true;
            }

            if (!data.arrivedT)
            {
                // Travel phase: either we make it to the anchor or we give up.
                if (info.HasStatusPersisted(statusDuelSpotTravelDuration))
                {
                    EndDuelSpotHangout(botAI, "travel phase timed out");
                    return true;
                }

                if (data.teleported && bot->GetMapId() == data.anchorPos.GetMapId() &&
                    bot->GetExactDist(data.anchorPos) < 20.0f)
                {
                    data.arrivedT = getMSTime();
                    data.dwellDuration = urand(sPlayerbotAIConfig.duelSpotDwellMinutesMin,
                                               sPlayerbotAIConfig.duelSpotDwellMinutesMax) *
                                         MINUTE * IN_MILLISECONDS;
                }
            }
            else
            {
                // Dwell phase: go home on expiry, or if something yanked the
                // bot far away.
                if (GetMSTimeDiffToNow(data.arrivedT) > data.dwellDuration)
                {
                    EndDuelSpotHangout(botAI, "dwell time expired", /*walkIntoCity*/ true);
                    return true;
                }

                if (bot->GetMapId() != data.anchorPos.GetMapId() || bot->GetExactDist(data.anchorPos) > 3000.0f)
                {
                    EndDuelSpotHangout(botAI, "yanked far from the dwell anchor");
                    return true;
                }
            }
            break;
        }
        default:
            break;
    }
    return false;
}

bool NewRpgGoGrindAction::Execute(Event /*event*/)
{
    if (SearchQuestGiverAndAcceptOrReward())
        return true;
    if (auto* data = std::get_if<NewRpgInfo::GoGrind>(&botAI->rpgInfo.data))
    {
        if (MoveFarTo(data->pos))
            return true;
        // Small nudge so the next tick's MoveFarTo starts from a
        // slightly different position. Kept small so it doesn't look
        // like the bot is abandoning its destination.
        return MoveRandomNear(10.0f);
    }

    return false;
}

bool NewRpgGoCampAction::Execute(Event /*event*/)
{
    if (SearchQuestGiverAndAcceptOrReward())
        return true;

    if (auto* data = std::get_if<NewRpgInfo::GoCamp>(&botAI->rpgInfo.data))
    {
        if (MoveFarTo(data->pos))
            return true;
        return MoveRandomNear(10.0f);
    }

    return false;
}

bool NewRpgWanderRandomAction::Execute(Event /*event*/)
{
    if (SearchQuestGiverAndAcceptOrReward())
        return true;

    NewRpgInfo& info = botAI->rpgInfo;
    auto* dataPtr = std::get_if<NewRpgInfo::WanderRandom>(&info.data);
    if (!dataPtr)
        return false;
    auto& data = *dataPtr;

    // First tick after entering the status: anchor the leash where the bot
    // stands (ChangeToWanderRandom has no bot to read a position from).
    if (data.origin == WorldPosition())
    {
        data.origin = WorldPosition(bot);
        data.lastEventful = getMSTime();
    }

    if (bot->IsInCombat() || AI_VALUE(Unit*, "grind target"))
        data.lastEventful = getMSTime();
    else if (GetMSTimeDiffToNow(data.lastEventful) >= wanderDeadSpotTimeout)
    {
        // Nothing to fight here for a while - stop pacing a dead (or
        // crowded-out) spot and let the idle roll pick a new activity.
        info.ChangeToIdle();
        return true;
    }

    return MoveDriftNear(50.0f, data.heading, data.origin, wanderLeashRadius);
}

bool NewRpgWanderNpcAction::Execute(Event /*event*/)
{
    NewRpgInfo& info = botAI->rpgInfo;
    auto* dataPtr = std::get_if<NewRpgInfo::WanderNpc>(&info.data);
    if (!dataPtr)
        return false;
    auto& data = *dataPtr;
    if (!data.npcOrGo)
    {
        // No npc can be found, switch to IDLE
        ObjectGuid npcOrGo = ChooseNpcOrGameObjectToInteract();
        if (npcOrGo.IsEmpty())
        {
            info.ChangeToIdle();
            return true;
        }
        data.npcOrGo = npcOrGo;
        data.lastReach = 0;
        return true;
    }

    WorldObject* object = ObjectAccessor::GetWorldObject(*bot, data.npcOrGo);
    if (object && IsWithinInteractionDist(object))
    {
        if (!data.lastReach)
        {
            data.lastReach = getMSTime();
            if (bot->CanInteractWithQuestGiver(object))
                InteractWithNpcOrGameObjectForQuest(data.npcOrGo);
            return true;
        }

        if (data.lastReach && GetMSTimeDiffToNow(data.lastReach) < npcStayTime)
            return false;

        // has reached the npc for more than `npcStayTime`, select the next target
        data.npcOrGo = ObjectGuid();
        data.lastReach = 0;
    }
    else
    {
        if (MoveWorldObjectTo(data.npcOrGo))
            return true;
        // NPC pathing failed (random offset in a wall, mmap hiccup, etc).
        // Take a small random step so the next tick retries from a
        // different spot instead of staring at the NPC from afar.
        return MoveRandomNear(15.0f);
    }

    return true;
}

bool NewRpgDoQuestAction::Execute(Event /*event*/)
{
    if (SearchQuestGiverAndAcceptOrReward())
        return true;

    NewRpgInfo& info = botAI->rpgInfo;
    auto* dataPtr = std::get_if<NewRpgInfo::DoQuest>(&info.data);
    if (!dataPtr)
        return false;
    auto& data = *dataPtr;
    uint32 questId = data.questId;
    uint8 questStatus = bot->GetQuestStatus(questId);
    switch (questStatus)
    {
        case QUEST_STATUS_INCOMPLETE:
            return DoIncompleteQuest(data);
        case QUEST_STATUS_COMPLETE:
            return DoCompletedQuest(data);
        default:
            break;
    }
    info.ChangeToIdle();
    return true;
}

// Progress counter for one quest objective, mirroring the kill/cast vs
// collect split used throughout DoIncompleteQuest.
static uint32 GetObjectiveProgress(QuestStatusData const& q_status, int32 objectiveIdx)
{
    if (objectiveIdx < 0)
        return 0;
    if (objectiveIdx < QUEST_OBJECTIVES_COUNT)
        return q_status.CreatureOrGOCount[objectiveIdx];
    if (objectiveIdx < QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT)
        return q_status.ItemCount[objectiveIdx - QUEST_OBJECTIVES_COUNT];
    return 0;
}

bool NewRpgDoQuestAction::DoIncompleteQuest(NewRpgInfo::DoQuest& data)
{
    uint32 questId = data.questId;
    if (data.pos != WorldPosition())
    {
        /// @TODO: extract to a new function
        int32 currentObjective = data.objectiveIdx;
        // check if the objective has completed
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        const QuestStatusData& q_status = bot->getQuestStatusMap().at(questId);
        bool completed = true;
        if (currentObjective < QUEST_OBJECTIVES_COUNT)
        {
            if (q_status.CreatureOrGOCount[currentObjective] < quest->RequiredNpcOrGoCount[currentObjective])
                completed = false;
        }
        else if (currentObjective < QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT)
        {
            if (q_status.ItemCount[currentObjective - QUEST_OBJECTIVES_COUNT] <
                quest->RequiredItemCount[currentObjective - QUEST_OBJECTIVES_COUNT])
                completed = false;
        }
        // the current objective is completed, clear and find a new objective later
        if (completed)
        {
            data.lastReachPOI = 0;
            data.pos = WorldPosition();
            data.objectiveIdx = 0;
            data.heading = -1.0f;
            data.noProgressStays = 0;
        }
    }
    if (data.pos == WorldPosition())
    {
        std::vector<POIInfo> poiInfo;
        if (!GetQuestPOIPosAndObjectiveIdx(questId, poiInfo))
        {
            // can't find a poi pos to go, stop doing quest for now
            botAI->rpgInfo.ChangeToIdle();
            return true;
        }
        uint32 rndIdx = urand(0, poiInfo.size() - 1);
        G3D::Vector2 nearestPoi = poiInfo[rndIdx].pos;
        int32 objectiveIdx = poiInfo[rndIdx].objectiveIdx;

        float dx = nearestPoi.x, dy = nearestPoi.y;

        // z = MAX_HEIGHT as we do not know accurate z
        float dz = std::max(bot->GetMap()->GetHeight(dx, dy, MAX_HEIGHT), bot->GetMap()->GetWaterLevel(dx, dy));

        // double check for GetQuestPOIPosAndObjectiveIdx
        if (dz == INVALID_HEIGHT || dz == VMAP_INVALID_HEIGHT_VALUE)
            return false;

        WorldPosition pos(bot->GetMapId(), dx, dy, dz);
        data.lastReachPOI = 0;
        data.pos = pos;
        data.objectiveIdx = objectiveIdx;
    }

    if (bot->GetDistance(data.pos) > 10.0f && !data.lastReachPOI)
    {
        if (MoveFarTo(data.pos))
            return true;
        // Long-range sampler couldn't land a candidate — nudge the
        // bot a short distance so the next tick retries from a
        // different position instead of sitting idle.
        return MoveRandomNear(10.0f);
    }
    // Now we are near the quest objective
    // kill mobs and looting quest should be done automatically by grind strategy

    if (!data.lastReachPOI)
    {
        data.lastReachPOI = getMSTime();
        data.progressAtReach = GetObjectiveProgress(bot->getQuestStatusMap().at(questId), data.objectiveIdx);
        return true;
    }
    // worked this POI point long enough - rotate to another of the quest's
    // points instead of camping one tile for minutes
    if (GetMSTimeDiffToNow(data.lastReachPOI) >= poiRotateTime)
    {
        QuestStatusData const& q_status = bot->getQuestStatusMap().at(questId);
        bool progressed = GetObjectiveProgress(q_status, data.objectiveIdx) > data.progressAtReach;
        data.noProgressStays = progressed ? 0 : data.noProgressStays + 1;
        if (data.noProgressStays >= poiMaxNoProgressStays)
        {
            // several consecutive stays without a single objective tick:
            // spawns contested by a crowd or a quest the bot can't actually
            // progress - abandon it
            /// @TODO: It may be better to make lowPriorityQuest a global set shared by all bots (or saved in db)
            botAI->lowPriorityQuest.insert(questId);
            botAI->rpgStatistic.questAbandoned++;
            LOG_DEBUG("playerbots", "[New RPG] {} marked as abandoned quest {}", bot->GetName(), questId);
            botAI->rpgInfo.ChangeToIdle();
            return true;
        }
        // clear and select another poi later
        data.lastReachPOI = 0;
        data.pos = WorldPosition();
        data.objectiveIdx = 0;
        data.heading = -1.0f;
        return true;
    }

    // At the POI: patrol the objective area while grind/loot strategies do
    // their work. Drift with a leash around the POI point - wide enough to
    // reach spawns the old tight 8yd shuffle never approached, anchored
    // enough to stay recognizably "at" the objective.
    return MoveDriftNear(20.0f, data.heading, data.pos, 40.0f);
}

bool NewRpgDoQuestAction::DoCompletedQuest(NewRpgInfo::DoQuest& data)
{
    uint32 questId = data.questId;
    const Quest* quest = data.quest;

    if (data.objectiveIdx != -1)
    {
        // if quest is completed, back to poi with -1 idx to reward
        BroadcastHelper::BroadcastQuestUpdateComplete(botAI, bot, quest);
        botAI->rpgStatistic.questCompleted++;
        std::vector<POIInfo> poiInfo;
        if (!GetQuestPOIPosAndObjectiveIdx(questId, poiInfo, true))
        {
            // can't find a poi pos to reward, stop doing quest for now
            botAI->rpgInfo.ChangeToIdle();
            return false;
        }
        assert(poiInfo.size() > 0);
        // now we get the place to get rewarded
        float dx = poiInfo[0].pos.x, dy = poiInfo[0].pos.y;
        // z = MAX_HEIGHT as we do not know accurate z
        float dz = std::max(bot->GetMap()->GetHeight(dx, dy, MAX_HEIGHT), bot->GetMap()->GetWaterLevel(dx, dy));

        // double check for GetQuestPOIPosAndObjectiveIdx
        if (dz == INVALID_HEIGHT || dz == VMAP_INVALID_HEIGHT_VALUE)
            return false;

        WorldPosition pos(bot->GetMapId(), dx, dy, dz);
        data.lastReachPOI = 0;
        data.pos = pos;
        data.objectiveIdx = -1;
    }

    if (data.pos == WorldPosition())
        return false;

    if (bot->GetDistance(data.pos) > 10.0f && !data.lastReachPOI)
    {
        if (MoveFarTo(data.pos))
            return true;
        return MoveRandomNear(10.0f);
    }

    // Now we are near the qoi of reward
    // the quest should be rewarded by SearchQuestGiverAndAcceptOrReward
    if (!data.lastReachPOI)
    {
        data.lastReachPOI = getMSTime();
        return true;
    }
    // stayed at this POI for more than 5 minutes
    if (GetMSTimeDiffToNow(data.lastReachPOI) >= poiStayTime)
    {
        // e.g. Can not reward quest to gameobjects
        /// @TODO: It may be better to make lowPriorityQuest a global set shared by all bots (or saved in db)
        botAI->lowPriorityQuest.insert(questId);
        botAI->rpgStatistic.questAbandoned++;
        LOG_DEBUG("playerbots", "[New RPG] {} marked as abandoned quest {}", bot->GetName(), questId);
        botAI->rpgInfo.ChangeToIdle();
        return true;
    }
    return false;
}

bool NewRpgTravelFlightAction::Execute(Event /*event*/)
{
    NewRpgInfo& info = botAI->rpgInfo;
    auto* dataPtr = std::get_if<NewRpgInfo::TravelFlight>(&info.data);
    if (!dataPtr)
        return false;

    auto& data = *dataPtr;
    if (bot->IsInFlight())
    {
        data.inFlight = true;
        return false;
    }

    if (bot->GetDistance(data.flightMasterPos) > INTERACTION_DISTANCE)
        return MoveFarTo(data.flightMasterPos);

    Creature* flightMaster = bot->FindNearestCreature(data.flightMasterEntry, INTERACTION_DISTANCE * 3);
    if (!flightMaster || !flightMaster->IsAlive())
    {
        info.ChangeToIdle();
        return true;
    }
    if (bot->GetDistance(flightMaster) > INTERACTION_DISTANCE)
        return MoveFarTo(flightMaster);

    std::vector<uint32> nodes = data.path;

    botAI->RemoveShapeshift();
    if (bot->IsMounted())
        bot->Dismount();

    bot->GetSession()->SendLearnNewTaxiNode(flightMaster);

    if (!bot->ActivateTaxiPathTo(nodes, flightMaster, 0))
    {
        LOG_DEBUG("playerbots", "[New RPG] {} active taxi path {} (from {} to {}) failed", bot->GetName(),
                  flightMaster->GetEntry(), nodes[0], nodes[nodes.size() - 1]);
        info.ChangeToIdle();
        return true;
    }
    return true;
}

bool NewRpgGoMoongladeAction::Execute(Event /*event*/)
{
    if (!std::get_if<NewRpgInfo::GoMoonglade>(&botAI->rpgInfo.data))
        return false;

    // The 10s cast is underway: let it finish. Arrival in Moonglade is
    // handled by NewRpgStatusUpdateAction.
    if (bot->IsNonMeleeSpellCast(false))
        return true;

    // Combat strategies take priority; the status times out if this drags on.
    if (bot->IsInCombat())
        return false;

    botAI->RemoveShapeshift();
    if (bot->IsMounted())
        bot->Dismount();

    // Casts with a cast time fail while moving.
    if (bot->isMoving())
        bot->StopMoving();

    if (!botAI->CanCastSpell(SPELL_TELEPORT_MOONGLADE, bot, true))
        return false;

    LOG_DEBUG("playerbots", "[New RPG] {} casting Teleport: Moonglade", bot->GetName());
    return botAI->CastSpell(SPELL_TELEPORT_MOONGLADE, bot);
}
