#include "NewRpgAction.h"

#include <cmath>
#include <cstdlib>

#include "AreaDefines.h"
#include "BroadcastHelper.h"
#include "ChatHelper.h"
#include "G3D/Vector2.h"
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
#include "QuestDef.h"
#include "Random.h"
#include "RandomPlayerbotMgr.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "TradeOfferMgr.h"
#include "TravelMgr.h"
#include "WpvpDefense.h"

bool TellRpgStatusAction::Execute(Event event)
{
    Player* owner = event.getOwner();
    if (!owner)
        return false;
    std::string out = botAI->rpgInfo.ToString();
    bot->Whisper(out.c_str(), LANG_UNIVERSAL, owner);
    return true;
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
                // bot far away (e.g. the death-count inn teleport).
                if (GetMSTimeDiffToNow(data.arrivedT) > data.dwellDuration)
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
                        // Help has arrived: while a live defender who
                        // outclasses the attacker shares the zone, the board
                        // holds WorldDefense escalation pleas (the board
                        // itself screens out reinforcers, who dwell on the
                        // attacker's own side, and even-fight arrivals).
                        if (bot->IsAlive())
                            WpvpDefenseBoard::instance().NoteDefenderOnScene(data.defendTarget, bot->GetTeamId(),
                                                                             bot->GetLevel());
                    }
                    else if (GetMSTimeDiffToNow(data.defendLastSeenT) > 90 * IN_MILLISECONDS)
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

    return MoveRandomNear();
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
        return true;
    }
    // stayed at this POI for more than 5 minutes
    if (GetMSTimeDiffToNow(data.lastReachPOI) >= poiStayTime)
    {
        bool hasProgression = false;
        int32 currentObjective = data.objectiveIdx;
        // check if the objective has progression
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        const QuestStatusData& q_status = bot->getQuestStatusMap().at(questId);
        if (currentObjective < QUEST_OBJECTIVES_COUNT)
        {
            if (q_status.CreatureOrGOCount[currentObjective] != 0 && quest->RequiredNpcOrGoCount[currentObjective])
                hasProgression = true;
        }
        else if (currentObjective < QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT)
        {
            if (q_status.ItemCount[currentObjective - QUEST_OBJECTIVES_COUNT] != 0 &&
                quest->RequiredItemCount[currentObjective - QUEST_OBJECTIVES_COUNT])
                hasProgression = true;
        }
        if (!hasProgression)
        {
            // we has reach the poi for more than 5 mins but no progession
            // may not be able to complete this quest, marked as abandoned
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
        return true;
    }

    // At the POI: keep the bot actively placed but avoid large
    // random 20yd hops that look like pacing back and forth. A small
    // ~8yd wander reads as the bot looking around while grind/loot
    // strategies do their work.
    return MoveRandomNear(8.0f);
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
