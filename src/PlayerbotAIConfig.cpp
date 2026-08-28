/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PlayerbotAIConfig.h"
#include "BisListMgr.h"
#include "Config.h"
#include "NewRpgInfo.h"
#include "PlayerbotDungeonRepository.h"
#include "PlayerbotFactory.h"
#include "PlayerbotGuildMgr.h"
#include "Playerbots.h"
#include "RandomItemMgr.h"
#include "RandomPlayerbotFactory.h"
#include "RandomPlayerbotMgr.h"
#include "Talentspec.h"
#include "TravelMgr.h"
#include <cctype>
#include <iostream>
#include <sstream>

template <class T>
void LoadList(std::string const value, T& list)
{
    std::vector<std::string> ids = split(value, ',');
    for (std::vector<std::string>::iterator i = ids.begin(); i != ids.end(); i++)
    {
        uint32 id = atoi((*i).c_str());
        // if (!id)
        //     continue;
        list.push_back(id);
    }
}

template <class T>
void LoadSet(std::string const value, T& set)
{
    std::vector<std::string> ids = split(value, ',');
    for (std::vector<std::string>::iterator i = ids.begin(); i != ids.end(); i++)
    {
        uint32 id = atoi((*i).c_str());
        // if (!id)
        //     continue;
        set.insert(id);
    }
}

template <class T>
void LoadListString(std::string const value, T& list)
{
    std::vector<std::string> strings = split(value, ',');
    for (std::vector<std::string>::iterator i = strings.begin(); i != strings.end(); i++)
    {
        std::string const string = *i;
        if (string.empty())
            continue;

        list.push_back(string);
    }
}

// Parses a comma-separated, whitespace-tolerant bot name list (as used by both
// AiPlayerbot.LevelBrackets.ExcludeNames and AiPlayerbot.ResetBotLevel.ExcludeNames) into out.
static void ParseLevelMgrExcludeNames(std::string const& csv, std::vector<std::string>& out)
{
    out.clear();
    std::istringstream f(csv);
    std::string s;
    while (getline(f, s, ','))
    {
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); }), s.end());
        if (!s.empty())
            out.push_back(s);
    }
}

bool PlayerbotAIConfig::Initialize()
{
    LOG_INFO("server.loading", "Initializing mod-playerbots, based on AI Playerbots by ike3 and the original Playerbots by blueboy");

    enabled = sConfigMgr->GetOption<bool>("AiPlayerbot.Enabled", true);
    if (!enabled)
    {
        LOG_INFO("server.loading", "Playerbots Module is disabled in playerbots.conf");
        return false;
    }

    globalCoolDown = sConfigMgr->GetOption<int32>("AiPlayerbot.GlobalCooldown", 500);
    maxWaitForMove = sConfigMgr->GetOption<int32>("AiPlayerbot.MaxWaitForMove", 5000);
    disableMoveSplinePath = sConfigMgr->GetOption<int32>("AiPlayerbot.DisableMoveSplinePath", 0);
    maxMovementSearchTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MaxMovementSearchTime", 3);
    expireActionTime = sConfigMgr->GetOption<int32>("AiPlayerbot.ExpireActionTime", 5000);
    dispelAuraDuration = sConfigMgr->GetOption<int32>("AiPlayerbot.DispelAuraDuration", 700);
    reactDelay = sConfigMgr->GetOption<int32>("AiPlayerbot.ReactDelay", 100);
    dynamicReactDelay = sConfigMgr->GetOption<bool>("AiPlayerbot.DynamicReactDelay", true);
    passiveDelay = sConfigMgr->GetOption<int32>("AiPlayerbot.PassiveDelay", 10000);
    repeatDelay = sConfigMgr->GetOption<int32>("AiPlayerbot.RepeatDelay", 2000);
    errorDelay = sConfigMgr->GetOption<int32>("AiPlayerbot.ErrorDelay", 100);
    rpgDelay = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgDelay", 10000);
    sitDelay = sConfigMgr->GetOption<int32>("AiPlayerbot.SitDelay", 20000);
    returnDelay = sConfigMgr->GetOption<int32>("AiPlayerbot.ReturnDelay", 2000);
    lootDelay = sConfigMgr->GetOption<int32>("AiPlayerbot.LootDelay", 1000);
    reactionDelayMin[REACTION_NONE] = 0;
    reactionDelayMax[REACTION_NONE] = 0;
    reactionMissChance[REACTION_NONE] = 0;
    reactionDelayMin[REACTION_INTERRUPT] = sConfigMgr->GetOption<uint32>("AiPlayerbot.ReactionDelayInterruptMin", 200);
    reactionDelayMax[REACTION_INTERRUPT] = sConfigMgr->GetOption<uint32>("AiPlayerbot.ReactionDelayInterruptMax", 1500);
    reactionMissChance[REACTION_INTERRUPT] = sConfigMgr->GetOption<uint32>("AiPlayerbot.ReactionMissChanceInterrupt", 5);
    reactionDelayMin[REACTION_DISPEL] = sConfigMgr->GetOption<uint32>("AiPlayerbot.ReactionDelayDispelMin", 400);
    reactionDelayMax[REACTION_DISPEL] = sConfigMgr->GetOption<uint32>("AiPlayerbot.ReactionDelayDispelMax", 2500);
    reactionMissChance[REACTION_DISPEL] = sConfigMgr->GetOption<uint32>("AiPlayerbot.ReactionMissChanceDispel", 5);
    reactionDelayMin[REACTION_EMERGENCY_HEAL] = sConfigMgr->GetOption<uint32>("AiPlayerbot.ReactionDelayEmergencyHealMin", 300);
    reactionDelayMax[REACTION_EMERGENCY_HEAL] = sConfigMgr->GetOption<uint32>("AiPlayerbot.ReactionDelayEmergencyHealMax", 2000);
    reactionMissChance[REACTION_EMERGENCY_HEAL] = sConfigMgr->GetOption<uint32>("AiPlayerbot.ReactionMissChanceEmergencyHeal", 5);
    disabledWithoutRealPlayerLoginDelay = sConfigMgr->GetOption<int32>("AiPlayerbot.DisabledWithoutRealPlayerLoginDelay", 30);
    disabledWithoutRealPlayerLogoutDelay = sConfigMgr->GetOption<int32>("AiPlayerbot.DisabledWithoutRealPlayerLogoutDelay", 300);

    farDistance = sConfigMgr->GetOption<float>("AiPlayerbot.FarDistance", 20.0f);
    sightDistance = sConfigMgr->GetOption<float>("AiPlayerbot.SightDistance", 100.0f);
    spellDistance = sConfigMgr->GetOption<float>("AiPlayerbot.SpellDistance", 28.5f);
    shootDistance = sConfigMgr->GetOption<float>("AiPlayerbot.ShootDistance", 5.0f);
    healDistance = sConfigMgr->GetOption<float>("AiPlayerbot.HealDistance", 38.5f);
    lootDistance = sConfigMgr->GetOption<float>("AiPlayerbot.LootDistance", 15.0f);
    fleeDistance = sConfigMgr->GetOption<float>("AiPlayerbot.FleeDistance", 5.0f);
    aggroDistance = sConfigMgr->GetOption<float>("AiPlayerbot.AggroDistance", 22.0f);
    tooCloseDistance = sConfigMgr->GetOption<float>("AiPlayerbot.TooCloseDistance", 5.0f);
    meleeDistance = sConfigMgr->GetOption<float>("AiPlayerbot.MeleeDistance", 0.75f);
    followDistance = sConfigMgr->GetOption<float>("AiPlayerbot.FollowDistance", 1.5f);
    whisperDistance = sConfigMgr->GetOption<float>("AiPlayerbot.WhisperDistance", 6000.0f);
    contactDistance = sConfigMgr->GetOption<float>("AiPlayerbot.ContactDistance", 0.45f);
    aoeRadius = sConfigMgr->GetOption<float>("AiPlayerbot.AoeRadius", 10.0f);
    rpgDistance = sConfigMgr->GetOption<float>("AiPlayerbot.RpgDistance", 200.0f);
    grindDistance = sConfigMgr->GetOption<float>("AiPlayerbot.GrindDistance", 75.0f);
    reactDistance = sConfigMgr->GetOption<float>("AiPlayerbot.ReactDistance", 150.0f);

    criticalHealth = sConfigMgr->GetOption<int32>("AiPlayerbot.CriticalHealth", 25);
    lowHealth = sConfigMgr->GetOption<int32>("AiPlayerbot.LowHealth", 45);
    mediumHealth = sConfigMgr->GetOption<int32>("AiPlayerbot.MediumHealth", 65);
    almostFullHealth = sConfigMgr->GetOption<int32>("AiPlayerbot.AlmostFullHealth", 85);
    lowMana = sConfigMgr->GetOption<int32>("AiPlayerbot.LowMana", 15);
    mediumMana = sConfigMgr->GetOption<int32>("AiPlayerbot.MediumMana", 40);
    highMana = sConfigMgr->GetOption<int32>("AiPlayerbot.HighMana", 65);
    autoSaveMana = sConfigMgr->GetOption<bool>("AiPlayerbot.AutoSaveMana", true);
    saveManaThreshold = sConfigMgr->GetOption<int32>("AiPlayerbot.SaveManaThreshold", 60);
    switch (sConfigMgr->GetOption<uint32>("AiPlayerbot.AutoGreaterBlessings", 1))
    {
        case 0:
            autoGreaterBlessings = AutoPartyBuffMode::DISABLED;
            break;
        case 2:
            autoGreaterBlessings = AutoPartyBuffMode::GROUP_OR_RAID;
            break;
        case 1:
        default:
            autoGreaterBlessings = AutoPartyBuffMode::RAID_ONLY;
            break;
    }
    switch (sConfigMgr->GetOption<uint32>("AiPlayerbot.AutoPartyBuffs", 2))
    {
        case 0:
            autoPartyBuffs = AutoPartyBuffMode::DISABLED;
            break;
        case 1:
            autoPartyBuffs = AutoPartyBuffMode::RAID_ONLY;
            break;
        case 2:
        default:
            autoPartyBuffs = AutoPartyBuffMode::GROUP_OR_RAID;
            break;
    }
    tellWhenMissingBuffReagents = sConfigMgr->GetOption<bool>("AiPlayerbot.TellWhenMissingBuffReagents", true);
    missingBuffReagentMessageCooldown = sConfigMgr->GetOption<uint32>(
        "AiPlayerbot.MissingBuffReagentMessageCooldown", 300);
    autoAvoidAoe = sConfigMgr->GetOption<bool>("AiPlayerbot.AutoAvoidAoe", true);
    maxAoeAvoidRadius = sConfigMgr->GetOption<float>("AiPlayerbot.MaxAoeAvoidRadius", 15.0f);
    LoadSet<std::set<uint32>>(sConfigMgr->GetOption<std::string>("AiPlayerbot.AoeAvoidSpellWhitelist", "50759,57491,13810,29946"),
                              aoeAvoidSpellWhitelist);
    tellWhenAvoidAoe = sConfigMgr->GetOption<bool>("AiPlayerbot.TellWhenAvoidAoe", false);

    randomGearLoweringChance = sConfigMgr->GetOption<float>("AiPlayerbot.RandomGearLoweringChance", 0.0f);
    randomGearQualityLimit = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomGearQualityLimit", 3);
    randomGearScoreLimit = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomGearScoreLimit", 0);
    preferClassArmorType  = sConfigMgr->GetOption<bool>("AiPlayerbot.PreferClassArmorType", false);
    preferredSpecWeapons  = sConfigMgr->GetOption<bool>("AiPlayerbot.PreferredSpecWeapons", false);

    randomBotMinLevelChance = sConfigMgr->GetOption<float>("AiPlayerbot.RandomBotMinLevelChance", 0.1f);
    randomBotMaxLevelChance = sConfigMgr->GetOption<float>("AiPlayerbot.RandomBotMaxLevelChance", 0.1f);
    randomBotRpgChance = sConfigMgr->GetOption<float>("AiPlayerbot.RandomBotRpgChance", 0.20f);

    iterationsPerTick = sConfigMgr->GetOption<int32>("AiPlayerbot.IterationsPerTick", 10);

    allowAccountBots = sConfigMgr->GetOption<bool>("AiPlayerbot.AllowAccountBots", true);
    allowGuildBots = sConfigMgr->GetOption<bool>("AiPlayerbot.AllowGuildBots", true);
    allowTrustedAccountBots = sConfigMgr->GetOption<bool>("AiPlayerbot.AllowTrustedAccountBots", true);
    disabledWithoutRealPlayer = sConfigMgr->GetOption<bool>("AiPlayerbot.DisabledWithoutRealPlayer", false);
    randomBotGuildNearby = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotGuildNearby", false);
    randomBotInvitePlayer = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotInvitePlayer", false);
    inviteChat = sConfigMgr->GetOption<bool>("AiPlayerbot.InviteChat", false);

    randomBotMapsAsString = sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotMaps", "0,1,530,571");
    LoadList<std::vector<uint32>>(randomBotMapsAsString, randomBotMaps);
    probTeleToBankers = sConfigMgr->GetOption<float>("AiPlayerbot.ProbTeleToBankers", 0.25f);
    enableWeightTeleToCityBankers = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableWeightTeleToCityBankers", false);
    weightTeleToStormwind = sConfigMgr->GetOption<int>("AiPlayerbot.TeleToStormwindWeight", 2);
    weightTeleToIronforge = sConfigMgr->GetOption<int>("AiPlayerbot.TeleToIronforgeWeight", 1);
    weightTeleToDarnassus = sConfigMgr->GetOption<int>("AiPlayerbot.TeleToDarnassusWeight", 1);
    weightTeleToExodar = sConfigMgr->GetOption<int>("AiPlayerbot.TeleToExodarWeight", 1);
    weightTeleToOrgrimmar = sConfigMgr->GetOption<int>("AiPlayerbot.TeleToOrgrimmarWeight", 2);
    weightTeleToUndercity = sConfigMgr->GetOption<int>("AiPlayerbot.TeleToUndercityWeight", 1);
    weightTeleToThunderBluff = sConfigMgr->GetOption<int>("AiPlayerbot.TeleToThunderBluffWeight", 1);
    weightTeleToSilvermoonCity = sConfigMgr->GetOption<int>("AiPlayerbot.TeleToSilvermoonCityWeight", 1);
    weightTeleToShattrathCity = sConfigMgr->GetOption<int>("AiPlayerbot.TeleToShattrathCityWeight", 1);
    weightTeleToDalaran = sConfigMgr->GetOption<int>("AiPlayerbot.TeleToDalaranWeight", 1);
    LoadList<std::vector<uint32>>(
        sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotQuestItems",
                                           "5175,5176,5177,5178,6948,11000,12382,13704,16309"),
        randomBotQuestItems);
    LoadList<std::vector<uint32>>(sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotSpellIds", "54197"),
                                  randomBotSpellIds);
    LoadList<std::vector<uint32>>(
        sConfigMgr->GetOption<std::string>("AiPlayerbot.PvpProhibitedZoneIds",
                                           "2255,656,2361,2362,2363,976,35,2268,3425,392,541,1446,3828,3712,3738,3565,"
                                           "3539,3623,4152,3988,4658,4284,4418,4436,4275,4323,4395,3703,4298,3951"),
        pvpProhibitedZoneIds);
    LoadList<std::vector<uint32>>(
        sConfigMgr->GetOption<std::string>("AiPlayerbot.PvpProhibitedAreaIds",
                                           "976,35,392,2268,4161,4010,4317,4312,3649,3887,3958,3724,4080,3938,3754,3786,"
                                           "3973,4085,4086,4087,4088"),
        pvpProhibitedAreaIds);
    fastReactInBG = sConfigMgr->GetOption<bool>("AiPlayerbot.FastReactInBG", true);
    LoadList<std::vector<uint32>>(
        sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotQuestIds", "3802,5505,6502,7761,7848,10277,10285,11492,"
                                           "13188,13189,24499,24511,24710,24712"),
        randomBotQuestIds);

    LoadSet<std::set<uint32>>(
        sConfigMgr->GetOption<std::string>("AiPlayerbot.DisallowedGameObjects",
                                           "176213,17155,2656,74448,19020,3719,3658,3705,3706,105579,75293,2857,"
                                           "179490,141596,160836,160845,179516,176224,181085,176112,128308,128403,"
                                           "165739,165738,175245,175970,176325,176327,123329,2560"),
        disallowedGameObjects);
    LoadSet<std::set<uint32>>(
        sConfigMgr->GetOption<std::string>("AiPlayerbot.AttunementQuests", "10279,10277,10282,10283,10284,10285,10296,"
                                           "10297,10298,11481,11482,11488,11490,11492,10901,10888,10445,10985"),
        attunementQuests);

    LoadSet<std::set<uint32>>(
        sConfigMgr->GetOption<std::string>("AiPlayerbot.UnobtainableItems", "12468,44869,44870,46978"),
        unobtainableItems);

    botAutologin = sConfigMgr->GetOption<bool>("AiPlayerbot.BotAutologin", false);
    randomBotAutologin = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotAutologin", true);
    minRandomBots = sConfigMgr->GetOption<int32>("AiPlayerbot.MinRandomBots", 500);
    maxRandomBots = sConfigMgr->GetOption<int32>("AiPlayerbot.MaxRandomBots", 500);
    randomBotUpdateInterval = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotUpdateInterval", 20);
    randomBotCountChangeMinInterval =
        sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotCountChangeMinInterval", 30 * MINUTE);
    randomBotCountChangeMaxInterval =
        sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotCountChangeMaxInterval", 2 * HOUR);
    minRandomBotInWorldTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MinRandomBotInWorldTime", 2 * HOUR);
    maxRandomBotInWorldTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MaxRandomBotInWorldTime", 14 * 24 * HOUR);
    minRandomBotRandomizeTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MinRandomBotRandomizeTime", 2 * HOUR);
    maxRandomBotRandomizeTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MaxRandomBotRandomizeTime", 14 * 24 * HOUR);
    minRandomBotChangeStrategyTime =
        sConfigMgr->GetOption<int32>("AiPlayerbot.MinRandomBotChangeStrategyTime", 30 * MINUTE);
    maxRandomBotChangeStrategyTime =
        sConfigMgr->GetOption<int32>("AiPlayerbot.MaxRandomBotChangeStrategyTime", 2 * HOUR);
    minRandomBotReviveTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MinRandomBotReviveTime", MINUTE);
    maxRandomBotReviveTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MaxRandomBotReviveTime", 5 * MINUTE);
    minRandomBotTeleportInterval = sConfigMgr->GetOption<int32>("AiPlayerbot.MinRandomBotTeleportInterval", 1 * HOUR);
    maxRandomBotTeleportInterval = sConfigMgr->GetOption<int32>("AiPlayerbot.MaxRandomBotTeleportInterval", 5 * HOUR);
    permanentlyInWorldTime =
        sConfigMgr->GetOption<int32>("AiPlayerbot.PermanentlyInWorldTime", 1 * YEAR);
    randomBotTeleportDistance = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotTeleportDistance", 100);
    randomBotsPerInterval = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotsPerInterval", 60);
    randomBotPrintStatsInterval = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotPrintStatsInterval", 300);
    minRandomBotsPriceChangeInterval =
        sConfigMgr->GetOption<int32>("AiPlayerbot.MinRandomBotsPriceChangeInterval", 2 * HOUR);
    maxRandomBotsPriceChangeInterval =
        sConfigMgr->GetOption<int32>("AiPlayerbot.MaxRandomBotsPriceChangeInterval", 48 * HOUR);
    randomBotJoinLfg = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotJoinLfg", true);

    restrictHealerDPS = sConfigMgr->GetOption<bool>("AiPlayerbot.HealerDPSMapRestriction", false);
    LoadList<std::vector<uint32>>(
        sConfigMgr->GetOption<std::string>("AiPlayerbot.RestrictedHealerDPSMaps",
                                             "33,34,36,43,47,48,70,90,109,129,209,229,230,329,349,389,429,1001,1004,"
                                             "1007,269,540,542,543,545,546,547,552,553,554,555,556,557,558,560,585,574,"
                                             "575,576,578,595,599,600,601,602,604,608,619,632,650,658,668,409,469,509,"
                                             "531,532,534,544,548,550,564,565,580,249,533,603,615,616,624,631,649,724"),
        restrictedHealerDPSMaps);

    //////////////////////////// ICC

    EnableICCBuffs = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableICCBuffs", true);

    //////////////////////////// Professions
    classMatchingProfessionChance =
        std::min<uint32>(100, sConfigMgr->GetOption<uint32>("AiPlayerbot.ClassMatchingProfessionChance", 30));
    fishingDistanceFromMaster = sConfigMgr->GetOption<float>("AiPlayerbot.FishingDistanceFromMaster", 10.0f);
    endFishingWithMaster = sConfigMgr->GetOption<float>("AiPlayerbot.EndFishingWithMaster", 30.0f);
    fishingDistance = sConfigMgr->GetOption<float>("AiPlayerbot.FishingDistance", 40.0f);
    enableFishingWithMaster = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableFishingWithMaster", true);
    //////////////////////////// CHAT
    enableBroadcasts = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableBroadcasts", true);
    randomBotTalk = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotTalk", false);
    randomBotEmote = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotEmote", false);
    emoteReplyChanceToBots =
        std::min<uint32>(100, sConfigMgr->GetOption<uint32>("AiPlayerbot.EmoteReplyChanceToBots", 30));  // 0-100
    emoteReplyClaimSeconds = sConfigMgr->GetOption<uint32>("AiPlayerbot.EmoteReplyClaimSeconds", 10);
    unpromptedEmoteChance =
        std::min<uint32>(100, sConfigMgr->GetOption<uint32>("AiPlayerbot.UnpromptedEmoteChance", 100));  // 0-100
    randomBotSuggestDungeons = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotSuggestDungeons", true);
    randomBotSayWithoutMaster = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotSayWithoutMaster", false);

    // broadcastChanceMaxValue is used in urand(1, broadcastChanceMaxValue) for broadcasts,
    // lowering it will increase the chance, setting it to 0 will disable broadcasts
    // for internal use, not intended to be change by the user
    broadcastChanceMaxValue = enableBroadcasts ? 30000 : 0;

    // all broadcast chances should be in range 1-broadcastChanceMaxValue, value of 0 will disable this particular
    // broadcast setting value to max does not guarantee the broadcast, as there are some internal randoms as well
    broadcastToGuildGlobalChance = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastToGuildGlobalChance", 30000);
    broadcastToWorldGlobalChance = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastToWorldGlobalChance", 30000);
    broadcastToGeneralGlobalChance = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastToGeneralGlobalChance", 30000);
    broadcastToTradeGlobalChance = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastToTradeGlobalChance", 30000);
    broadcastToLFGGlobalChance = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastToLFGGlobalChance", 30000);
    broadcastToLocalDefenseGlobalChance =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastToLocalDefenseGlobalChance", 30000);
    broadcastToWorldDefenseGlobalChance =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastToWorldDefenseGlobalChance", 30000);
    broadcastToGuildRecruitmentGlobalChance =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastToGuildRecruitmentGlobalChance", 30000);

    broadcastChanceLootingItemPoor = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceLootingItemPoor", 30);
    broadcastChanceLootingItemNormal =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceLootingItemNormal", 300);
    broadcastChanceLootingItemUncommon =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceLootingItemUncommon", 10000);
    broadcastChanceLootingItemRare = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceLootingItemRare", 20000);
    broadcastChanceLootingItemEpic = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceLootingItemEpic", 30000);
    broadcastChanceLootingItemLegendary =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceLootingItemLegendary", 30000);
    broadcastChanceLootingItemArtifact =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceLootingItemArtifact", 30000);

    broadcastChanceQuestAccepted = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceQuestAccepted", 6000);
    broadcastChanceQuestUpdateObjectiveCompleted =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceQuestUpdateObjectiveCompleted", 300);
    broadcastChanceQuestUpdateObjectiveProgress =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceQuestUpdateObjectiveProgress", 300);
    broadcastChanceQuestUpdateFailedTimer =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceQuestUpdateFailedTimer", 300);
    broadcastChanceQuestUpdateComplete =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceQuestUpdateComplete", 1000);
    broadcastChanceQuestTurnedIn = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceQuestTurnedIn", 10000);

    broadcastChanceKillNormal = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceKillNormal", 30);
    broadcastChanceKillElite = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceKillElite", 300);
    broadcastChanceKillRareelite = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceKillRareelite", 3000);
    broadcastChanceKillWorldboss = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceKillWorldboss", 20000);
    broadcastChanceKillRare = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceKillRare", 10000);
    broadcastChanceKillUnknown = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceKillUnknown", 100);
    broadcastChanceKillPet = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceKillPet", 10);
    broadcastChanceKillPlayer = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceKillPlayer", 30);

    broadcastChanceLevelupGeneric = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceLevelupGeneric", 20000);
    broadcastChanceLevelupTenX = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceLevelupTenX", 30000);
    broadcastChanceLevelupMaxLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceLevelupMaxLevel", 30000);

    broadcastChanceSuggestInstance = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceSuggestInstance", 5000);
    broadcastChanceSuggestQuest = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceSuggestQuest", 10000);
    broadcastChanceSuggestGrindMaterials =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceSuggestGrindMaterials", 5000);
    broadcastChanceSuggestGrindReputation =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceSuggestGrindReputation", 5000);
    broadcastChanceSuggestSell = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceSuggestSell", 300);
    broadcastChanceSuggestSomething =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceSuggestSomething", 30000);

    broadcastChanceSuggestSomethingToxic =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceSuggestSomethingToxic", 0);

    broadcastChanceSuggestToxicLinks = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceSuggestToxicLinks", 0);
    toxicLinksPrefix = sConfigMgr->GetOption<std::string>("AiPlayerbot.ToxicLinksPrefix", "gnomes");

    broadcastChanceSuggestThunderfury =
        sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceSuggestThunderfury", 1);

    // does not depend on global chance
    broadcastChanceGuildManagement = sConfigMgr->GetOption<int32>("AiPlayerbot.BroadcastChanceGuildManagement", 30000);

    toxicLinksRepliesChance = sConfigMgr->GetOption<int32>("AiPlayerbot.ToxicLinksRepliesChance", 30);    // 0-100
    thunderfuryRepliesChance = sConfigMgr->GetOption<int32>("AiPlayerbot.ThunderfuryRepliesChance", 40);  // 0-100
    guildRepliesRate = sConfigMgr->GetOption<int32>("AiPlayerbot.GuildRepliesRate", 100);                 // 0-100

    randomBotJoinBG = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotJoinBG", true);
    randomBotAutoJoinBG = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotAutoJoinBG", false);

    randomBotAutoJoinArenaBracket = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotAutoJoinArenaBracket", 14);

    randomBotAutoJoinWSBrackets = sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotAutoJoinWSBrackets", "7");
    randomBotAutoJoinABBrackets = sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotAutoJoinABBrackets", "6");
    randomBotAutoJoinAVBrackets = sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotAutoJoinAVBrackets", "3");
    randomBotAutoJoinEYBrackets = sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotAutoJoinEYBrackets", "2");
    randomBotAutoJoinICBrackets = sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotAutoJoinICBrackets", "1");

    randomBotAutoJoinBGWSCount = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotAutoJoinBGWSCount", 1);
    randomBotAutoJoinBGABCount = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotAutoJoinBGABCount", 1);
    randomBotAutoJoinBGAVCount = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotAutoJoinBGAVCount", 0);
    randomBotAutoJoinBGEYCount = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotAutoJoinBGEYCount", 1);
    randomBotAutoJoinBGICCount = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotAutoJoinBGICCount", 0);

    randomBotAutoJoinBGRatedArena2v2Count =
        sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotAutoJoinBGRatedArena2v2Count", 0);
    randomBotAutoJoinBGRatedArena3v3Count =
        sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotAutoJoinBGRatedArena3v3Count", 0);
    randomBotAutoJoinBGRatedArena5v5Count =
        sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotAutoJoinBGRatedArena5v5Count", 0);
    logInGroupOnly = sConfigMgr->GetOption<bool>("AiPlayerbot.LogInGroupOnly", true);
    logValuesPerTick = sConfigMgr->GetOption<bool>("AiPlayerbot.LogValuesPerTick", false);
    fleeingEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.FleeingEnabled", true);
    summonAtInnkeepersEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.SummonAtInnkeepersEnabled", true);
    randomBotMinLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotMinLevel", 1);
    randomBotMaxLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotMaxLevel", 80);
    if (randomBotMaxLevel > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        randomBotMaxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);

    // Bracket defaults (below) derive from randomBotMaxLevel, so this must run after it is read.
    LoadRandomBotLevelConfig();

    randomBotTeleLowerLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotTeleLowerLevel", 1);
    randomBotTeleHigherLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotTeleHigherLevel", 3);
    openGoSpell = sConfigMgr->GetOption<int32>("AiPlayerbot.OpenGoSpell", 6477);

    // Zones for NewRpgStrategy teleportation brackets
    std::vector<uint32> zoneIds = {
        // Classic WoW - Low-level zones
        1, 12, 14, 85, 141, 215, 3430, 3524,
        // Classic WoW - Mid-level zones
        17, 38, 40, 130, 148, 3433, 3525,
        // Classic WoW - High-level zones
        10, 11, 44, 267, 331, 400, 406,
        // Classic WoW - Higher-level zones
        3, 8, 15, 16, 33, 45, 47, 51, 357, 405, 440,
        // Classic WoW - Top-level zones
        4, 28, 46, 139, 361, 490, 618, 1377,
        // The Burning Crusade - Zones
        3483, 3518, 3519, 3520, 3521, 3522, 3523, 4080,
        // Wrath of the Lich King - Zones
        65, 66, 67, 210, 394, 495, 2817, 3537, 3711, 4197
    };

    for (uint32 zoneId : zoneIds)
    {
        std::string setting = "AiPlayerbot.ZoneBracket." + std::to_string(zoneId);
        std::string value = sConfigMgr->GetOption<std::string>(setting, "");

        if (!value.empty())
        {
            size_t commaPos = value.find(',');
            if (commaPos != std::string::npos)
            {
                uint32 minLevel = atoi(value.substr(0, commaPos).c_str());
                uint32 maxLevel = atoi(value.substr(commaPos + 1).c_str());
                zoneBrackets[zoneId] = std::make_pair(minLevel, maxLevel);
            }
        }
    }

    randomChangeMultiplier = sConfigMgr->GetOption<float>("AiPlayerbot.RandomChangeMultiplier", 1.0);

    randomBotCombatStrategies = sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotCombatStrategies", "");
    randomBotNonCombatStrategies = sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotNonCombatStrategies", "");
    combatStrategies = sConfigMgr->GetOption<std::string>("AiPlayerbot.CombatStrategies", "");
    nonCombatStrategies = sConfigMgr->GetOption<std::string>("AiPlayerbot.NonCombatStrategies", "");
    applyInstanceStrategies = sConfigMgr->GetOption<bool>("AiPlayerbot.ApplyInstanceStrategies", true);

    commandPrefix = sConfigMgr->GetOption<std::string>("AiPlayerbot.CommandPrefix", "");
    commandSeparator = sConfigMgr->GetOption<std::string>("AiPlayerbot.CommandSeparator", "\\\\");

    commandServerPort = sConfigMgr->GetOption<int32>("AiPlayerbot.CommandServerPort", 8888);
    perfMonEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.PerfMonEnabled", false);

    useGroundMountAtMinLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.UseGroundMountAtMinLevel", 20);
    useFastGroundMountAtMinLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.UseFastGroundMountAtMinLevel", 40);
    useFlyMountAtMinLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.UseFlyMountAtMinLevel", 60);
    useFastFlyMountAtMinLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.UseFastFlyMountAtMinLevel", 70);

    // stagger bot flightpath takeoff
    botTaxiDelayMin = sConfigMgr->GetOption<uint32>("AiPlayerbot.BotTaxiDelayMinMs", 350);
    botTaxiDelayMax = sConfigMgr->GetOption<uint32>("AiPlayerbot.BotTaxiDelayMaxMs", 5000);
    botTaxiGapMs = sConfigMgr->GetOption<uint32>("AiPlayerbot.BotTaxiGapMs", 200);
    botTaxiGapJitterMs = sConfigMgr->GetOption<uint32>("AiPlayerbot.BotTaxiGapJitterMs", 100);

    LOG_INFO("server.loading", "Loading TalentSpecs...");

    for (uint32 cls = 1; cls < MAX_CLASSES; ++cls)
    {
        if (cls == 10)
        {
            continue;
        }
        for (uint32 spec = 0; spec < MAX_SPECNO; ++spec)
        {
            std::ostringstream os;
            os << "AiPlayerbot.PremadeSpecName." << cls << "." << spec;
            premadeSpecName[cls][spec] = sConfigMgr->GetOption<std::string>(os.str().c_str(), "", false);
            os.str("");
            os.clear();
            os << "AiPlayerbot.PremadeSpecGlyph." << cls << "." << spec;
            premadeSpecGlyph[cls][spec] = sConfigMgr->GetOption<std::string>(os.str().c_str(), "", false);
            std::vector<std::string> splitSpecGlyph = split(premadeSpecGlyph[cls][spec], ',');
            for (std::string& split : splitSpecGlyph)
            {
                if (split.size() != 0)
                {
                    parsedSpecGlyph[cls][spec].push_back(atoi(split.c_str()));
                }
            }
            for (uint32 level = 0; level < MAX_LEVEL; ++level)
            {
                std::ostringstream os;
                os << "AiPlayerbot.PremadeSpecLink." << cls << "." << spec << "." << level;
                premadeSpecLink[cls][spec][level] = sConfigMgr->GetOption<std::string>(os.str().c_str(), "", false);
                parsedSpecLinkOrder[cls][spec][level] = ParseTempTalentsOrder(cls, premadeSpecLink[cls][spec][level]);
            }
        }
        for (uint32 spec = 0; spec < 3; ++spec)
        {
            for (uint32 points = 0; points < 21; ++points)
            {
                std::ostringstream os;
                os << "AiPlayerbot.PremadeHunterPetLink." << spec << "." << points;
                premadeHunterPetLink[spec][points] = sConfigMgr->GetOption<std::string>(os.str().c_str(), "", false);
                parsedHunterPetLinkOrder[spec][points] =
                    ParseTempPetTalentsOrder(spec, premadeHunterPetLink[spec][points]);
            }
        }
        for (uint32 spec = 0; spec < MAX_SPECNO; ++spec)
        {
            std::ostringstream os;
            os << "AiPlayerbot.RandomClassSpecProb." << cls << "." << spec;
            uint32 def;
            if (spec <= 1)
                def = 33;
            else if (spec == 2)
                def = 34;
            else
                def = 0;
            randomClassSpecProb[cls][spec] = sConfigMgr->GetOption<uint32>(os.str().c_str(), def, false);
            os.str("");
            os.clear();
            os << "AiPlayerbot.RandomClassSpecIndex." << cls << "." << spec;
            randomClassSpecIndex[cls][spec] = sConfigMgr->GetOption<uint32>(os.str().c_str(), spec, false);
        }
    }

    botCheats.clear();
    LoadListString<std::vector<std::string>>(sConfigMgr->GetOption<std::string>("AiPlayerbot.BotCheats", "food,taxi,raid"),
                                             botCheats);

    botCheatMask = 0;

    if (std::find(botCheats.begin(), botCheats.end(), "food") != botCheats.end())
        botCheatMask |= (uint32)BotCheatMask::food;
    if (std::find(botCheats.begin(), botCheats.end(), "taxi") != botCheats.end())
        botCheatMask |= (uint32)BotCheatMask::taxi;
    if (std::find(botCheats.begin(), botCheats.end(), "gold") != botCheats.end())
        botCheatMask |= (uint32)BotCheatMask::gold;
    if (std::find(botCheats.begin(), botCheats.end(), "health") != botCheats.end())
        botCheatMask |= (uint32)BotCheatMask::health;
    if (std::find(botCheats.begin(), botCheats.end(), "mana") != botCheats.end())
        botCheatMask |= (uint32)BotCheatMask::mana;
    if (std::find(botCheats.begin(), botCheats.end(), "power") != botCheats.end())
        botCheatMask |= (uint32)BotCheatMask::power;
    if (std::find(botCheats.begin(), botCheats.end(), "raid") != botCheats.end())
        botCheatMask |= (uint32)BotCheatMask::raid;

    LoadListString<std::vector<std::string>>(sConfigMgr->GetOption<std::string>("AiPlayerbot.AllowedLogFiles", ""),
                                             allowedLogFiles);
    enableAutoTradeOnItemMention = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableAutoTradeOnItemMention", true);
    LoadListString<std::vector<std::string>>(sConfigMgr->GetOption<std::string>("AiPlayerbot.TradeActionExcludedPrefixes", ""),
                                             tradeActionExcludedPrefixes);

    worldBuffs.clear();
    loadWorldBuff();
    LOG_INFO("playerbots", "Loading World Buff Feature...");

    randomBotAccountPrefix = sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotAccountPrefix", "rndbot");
    randomBotAccountCount = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotAccountCount", 0);
    deleteRandomBotAccounts = sConfigMgr->GetOption<bool>("AiPlayerbot.DeleteRandomBotAccounts", false);
    randomBotGuildCount = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotGuildCount", 20);
    randomBotGuildSizeMax = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotGuildSizeMax", 15);
    deleteRandomBotGuilds = sConfigMgr->GetOption<bool>("AiPlayerbot.DeleteRandomBotGuilds", false);

    botSendMailEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.BotSendMailEnabled", true);

    guildTaskEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableGuildTasks", false);
    minGuildTaskChangeTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MinGuildTaskChangeTime", 3 * 24 * 3600);
    maxGuildTaskChangeTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MaxGuildTaskChangeTime", 4 * 24 * 3600);
    minGuildTaskAdvertisementTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MinGuildTaskAdvertisementTime", 300);
    maxGuildTaskAdvertisementTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MaxGuildTaskAdvertisementTime", 12 * 3600);
    minGuildTaskRewardTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MinGuildTaskRewardTime", 300);
    maxGuildTaskRewardTime = sConfigMgr->GetOption<int32>("AiPlayerbot.MaxGuildTaskRewardTime", 3600);
    guildTaskAdvertCleanupTime = sConfigMgr->GetOption<int32>("AiPlayerbot.GuildTaskAdvertCleanupTime", 300);
    guildTaskKillTaskDistance = sConfigMgr->GetOption<int32>("AiPlayerbot.GuildTaskKillTaskDistance", 2000);
    targetPosRecalcDistance = sConfigMgr->GetOption<float>("AiPlayerbot.TargetPosRecalcDistance", 0.1f);

    //cosmetics
    switch (sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotShowHelmet", 1))
    {
        case 0:
            randomBotShowHelmet = ShowHideCosmetic::ALWAYS_HIDE;
            break;
        case 2:
            randomBotShowHelmet = ShowHideCosmetic::RANDOMIZE;
            break;
        case 1:
        default:
            randomBotShowHelmet = ShowHideCosmetic::ALWAYS_SHOW;
            break;
    }
    switch (sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotShowCloak", 1))
    {
        case 0:
            randomBotShowCloak = ShowHideCosmetic::ALWAYS_HIDE;
            break;
        case 2:
            randomBotShowCloak = ShowHideCosmetic::RANDOMIZE;
            break;
        case 1:
        default:
            randomBotShowCloak = ShowHideCosmetic::ALWAYS_SHOW;
            break;
    }

    // quest competition groups (Felworld)
    questCompetitionInvite = sConfigMgr->GetOption<bool>("AiPlayerbot.QuestCompetitionInvite", true);
    questCompetitionInviteCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.QuestCompetitionInviteCooldown", 600);
    // A party, never a raid: 2 is the bot and the player it grouped with,
    // which leaves no room to recruit other bots.
    questCompetitionGroupSize =
        std::clamp<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.QuestCompetitionGroupSize", 5), 2, 5);

    // dungeon pulls by the main tank (Felworld)
    dungeonPullByTank = sConfigMgr->GetOption<bool>("AiPlayerbot.DungeonPullByTank", true);
    dungeonPullGroupRange = sConfigMgr->GetOption<float>("AiPlayerbot.DungeonPullGroupRange", 30.0f);
    dungeonPullMinHealth = sConfigMgr->GetOption<uint32>("AiPlayerbot.DungeonPullMinHealth", 80);
    dungeonPullMinMana = sConfigMgr->GetOption<uint32>("AiPlayerbot.DungeonPullMinMana", 60);

    // holding fire until the main tank has the mob (Felworld)
    dungeonHoldForTank = sConfigMgr->GetOption<bool>("AiPlayerbot.DungeonHoldForTank", true);
    dungeonHoldEngageDelay = sConfigMgr->GetOption<uint32>("AiPlayerbot.DungeonHoldEngageDelay", 1500);
    dungeonHoldTimeout = sConfigMgr->GetOption<uint32>("AiPlayerbot.DungeonHoldTimeout", 5000);

    // bystander assist (Felworld)
    enableBystanderAssist = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableBystanderAssist", true);
    bystanderAssistRadius = sConfigMgr->GetOption<float>("AiPlayerbot.BystanderAssistRadius", 40.0f);
    bystanderDistressHealth = sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderDistressHealth", 40);
    bystanderDistressHealerHealth = sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderDistressHealerHealth", 25);
    bystanderDistressRateHealthLoss = sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderDistressRateHealthLoss", 20);
    bystanderDistressRateWindow = sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderDistressRateWindow", 4);
    bystanderDistressMobCount = sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderDistressMobCount", 3);
    bystanderDistressLowMana = sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderDistressLowMana", 15);
    bystanderDistressLowManaHealth = sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderDistressLowManaHealth", 60);
    bystanderAssistMaxHelpers = sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderAssistMaxHelpers", 2);
    bystanderAssistCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderAssistCooldown", 30);
    bystanderAssistSelfHealth = sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderAssistSelfHealth", 50);
    bystanderAssistSelfMana = sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderAssistSelfMana", 30);
    bystanderPvpSupportChance =
        std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.BystanderPvpSupportChance", 50), 100);

    // social buffing (Felworld)
    enableSocialBuffing = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableSocialBuffing", true);
    socialBuffRadius = sConfigMgr->GetOption<float>("AiPlayerbot.SocialBuffRadius", 30.0f);
    socialBuffCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.SocialBuffCooldown", 600);
    socialBuffGiverCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.SocialBuffGiverCooldown", 60);
    socialBuffSelfMana = sConfigMgr->GetOption<uint32>("AiPlayerbot.SocialBuffSelfMana", 40);
    enableHealThanks = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableHealThanks", true);
    socialThankCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.SocialThankCooldown", 180);

    // standoff against damage-immune enemy players (Felworld)
    enableImmunityStandoff = sConfigMgr->GetOption<bool>("AiPlayerbot.ImmunityStandoff", true);

    // stealth-spotting reactions (Felworld)
    enableStealthReactions = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableStealthReactions", true);
    stealthReactionCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.StealthReactionCooldown", 180);
    stealthReactionEmoteChance = sConfigMgr->GetOption<uint32>("AiPlayerbot.StealthReactionEmoteChance", 30);
    stealthFlushChance = std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.StealthFlushChance", 70), 100);
    stealthFlushSeconds = sConfigMgr->GetOption<uint32>("AiPlayerbot.StealthFlushSeconds", 15);
    rogueDistractChance = std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.RogueDistractChance", 60), 100);
    distractSuspicionChance =
        std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.DistractSuspicionChance", 60), 100);

    // SPP switches
    enableGreet = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableGreet", true);
    groupChatter = sConfigMgr->GetOption<bool>("AiPlayerbot.GroupChatter", true);
    groupChatterChance = std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.GroupChatterChance", 85), 100);
    groupChatterFalloff = std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.GroupChatterFalloff", 30), 100);
    engineeringChatter = sConfigMgr->GetOption<bool>("AiPlayerbot.EngineeringChatter", true);
    summonWhenGroup = sConfigMgr->GetOption<bool>("AiPlayerbot.SummonWhenGroup", true);
    randomBotFixedLevel = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotFixedLevel", false);
    disableRandomLevels = sConfigMgr->GetOption<bool>("AiPlayerbot.DisableRandomLevels", false);
    randomBotRandomPassword = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotRandomPassword", true);
    downgradeMaxLevelBot = sConfigMgr->GetOption<bool>("AiPlayerbot.DowngradeMaxLevelBot", true);
    equipAndSpecPersistence = sConfigMgr->GetOption<bool>("AiPlayerbot.EquipAndSpecPersistence", true);
    equipAndSpecPersistenceLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.EquipAndSpecPersistenceLevel", 1);
    groupInvitationPermission = sConfigMgr->GetOption<int32>("AiPlayerbot.GroupInvitationPermission", 1);
    keepAltsInGroup = sConfigMgr->GetOption<bool>("AiPlayerbot.KeepAltsInGroup", false);
    allowSummonInCombat = sConfigMgr->GetOption<bool>("AiPlayerbot.AllowSummonInCombat", true);
    allowSummonWhenMasterIsDead = sConfigMgr->GetOption<bool>("AiPlayerbot.AllowSummonWhenMasterIsDead", true);
    allowSummonWhenBotIsDead = sConfigMgr->GetOption<bool>("AiPlayerbot.AllowSummonWhenBotIsDead", true);
    reviveBotWhenSummoned = sConfigMgr->GetOption<int32>("AiPlayerbot.ReviveBotWhenSummoned", 1);
    botRepairWhenSummon = sConfigMgr->GetOption<bool>("AiPlayerbot.BotRepairWhenSummon", true);
    autoInitOnly = sConfigMgr->GetOption<bool>("AiPlayerbot.AutoInitOnly", false);
    resetInstanceIdForAltBots = sConfigMgr->GetOption<bool>("AiPlayerbot.ResetInstanceIdForAltBots", false);
    autoInitEquipLevelLimitRatio = sConfigMgr->GetOption<float>("AiPlayerbot.AutoInitEquipLevelLimitRatio", 1.0);

    maxAddedBots = sConfigMgr->GetOption<int32>("AiPlayerbot.MaxAddedBots", 40);
    addClassCommand = sConfigMgr->GetOption<int32>("AiPlayerbot.AddClassCommand", 1);
    addClassAccountPoolSize = sConfigMgr->GetOption<int32>("AiPlayerbot.AddClassAccountPoolSize", 50);
    maintenanceCommand = sConfigMgr->GetOption<int32>("AiPlayerbot.MaintenanceCommand", 1);

    altMaintenanceAttunementQs = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceAttunementQuests", true);
    altMaintenanceBags = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceBags", true);
    altMaintenanceAmmo = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceAmmo", true);
    altMaintenanceFood = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceFood", true);
    altMaintenanceReagents = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceReagents", true);
    altMaintenanceConsumables = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceConsumables", true);
    altMaintenancePotions = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenancePotions", true);
    altMaintenanceTalentTree = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceTalentTree", true);
    altMaintenancePet = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenancePet", true);
    altMaintenancePetTalents = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenancePetTalents", true);
    altMaintenanceClassSpells = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceClassSpells", true);
    altMaintenanceAvailableSpells = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceAvailableSpells", true);
    altMaintenanceSkills = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceSkills", true);
    altMaintenanceReputation = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceReputation", true);
    altMaintenanceSpecialSpells = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceSpecialSpells", true);
    altMaintenanceMounts = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceMounts", true);
    altMaintenanceGlyphs = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceGlyphs", true);
    altMaintenanceKeyring = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceKeyring", true);
    altMaintenanceGemsEnchants = sConfigMgr->GetOption<bool>("AiPlayerbot.AltMaintenanceGemsEnchants", true);

    autoGearCommand = sConfigMgr->GetOption<int32>("AiPlayerbot.AutoGearCommand", 1);
    autoGearCommandAltBots = sConfigMgr->GetOption<int32>("AiPlayerbot.AutoGearCommandAltBots", 1);
    autoGearBisCommand = sConfigMgr->GetOption<int32>("AiPlayerbot.AutoGearBisCommand", 0);
    autoGearQualityLimit = sConfigMgr->GetOption<int32>("AiPlayerbot.AutoGearQualityLimit", 3);
    autoGearScoreLimit = sConfigMgr->GetOption<int32>("AiPlayerbot.AutoGearScoreLimit", 0);

    randomBotXPRate = sConfigMgr->GetOption<float>("AiPlayerbot.RandomBotXPRate", 1.0);
    ghostMoveSpeedRate = sConfigMgr->GetOption<float>("AiPlayerbot.GhostMoveSpeedRate", 1.0);
    resurrectionSicknessLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.ResurrectionSicknessLevel", 0);
    castDeserter = sConfigMgr->GetOption<bool>("AiPlayerbot.CastDeserter", false);
    randomBotAllianceRatio = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotAllianceRatio", 50);
    randomBotHordeRatio = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotHordeRatio", 50);
    disableDeathKnightLogin = sConfigMgr->GetOption<bool>("AiPlayerbot.DisableDeathKnightLogin", 0);
    limitTalentsExpansion = sConfigMgr->GetOption<bool>("AiPlayerbot.LimitTalentsExpansion", 0);
    botActiveAlone = sConfigMgr->GetOption<int32>("AiPlayerbot.BotActiveAlone", 10);
    BotActiveAloneDurationSeconds = sConfigMgr->GetOption<int32>("AiPlayerbot.BotActiveAloneDurationSeconds", 30);
    BotActiveAloneForceWhenInRadius = sConfigMgr->GetOption<uint32>("AiPlayerbot.BotActiveAloneForceWhenInRadius", 150);
    BotActiveAloneForceWhenInZone = sConfigMgr->GetOption<bool>("AiPlayerbot.BotActiveAloneForceWhenInZone", 1);
    BotActiveAloneForceWhenInMap = sConfigMgr->GetOption<bool>("AiPlayerbot.BotActiveAloneForceWhenInMap", 0);
    BotActiveAloneForceWhenIsFriend = sConfigMgr->GetOption<bool>("AiPlayerbot.BotActiveAloneForceWhenIsFriend", 0);
    BotActiveAloneForceWhenInGuild = sConfigMgr->GetOption<bool>("AiPlayerbot.BotActiveAloneForceWhenInGuild", 1);
    botActiveAloneSmartScale = sConfigMgr->GetOption<bool>("AiPlayerbot.botActiveAloneSmartScale", 1);
    botActiveAloneSmartScaleDiffLimitfloor = sConfigMgr->GetOption<uint32>("AiPlayerbot.botActiveAloneSmartScaleDiffLimitfloor", 50);
    botActiveAloneSmartScaleDiffLimitCeiling = sConfigMgr->GetOption<uint32>("AiPlayerbot.botActiveAloneSmartScaleDiffLimitCeiling", 200);
    botActiveAloneSmartScaleWhenMinLevel = sConfigMgr->GetOption<uint32>("AiPlayerbot.botActiveAloneSmartScaleWhenMinLevel", 1);
    botActiveAloneSmartScaleWhenMaxLevel = sConfigMgr->GetOption<uint32>("AiPlayerbot.botActiveAloneSmartScaleWhenMaxLevel", 80);

    randombotsWalkingRPG = sConfigMgr->GetOption<bool>("AiPlayerbot.RandombotsWalkingRPG", false);
    randombotsWalkingRPGInDoors = sConfigMgr->GetOption<bool>("AiPlayerbot.RandombotsWalkingRPG.InDoors", false);
    minEnchantingBotLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.MinEnchantingBotLevel", 60);
    limitEnchantExpansion = sConfigMgr->GetOption<int32>("AiPlayerbot.LimitEnchantExpansion", 1);
    limitGearExpansion = sConfigMgr->GetOption<int32>("AiPlayerbot.LimitGearExpansion", 1);
    randombotStartingLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.RandombotStartingLevel", 1);
    enablePeriodicOnlineOffline = sConfigMgr->GetOption<bool>("AiPlayerbot.EnablePeriodicOnlineOffline", false);
    enableRandomBotTrading = sConfigMgr->GetOption<int32>("AiPlayerbot.EnableRandomBotTrading", 1);
    periodicOnlineOfflineRatio = sConfigMgr->GetOption<float>("AiPlayerbot.PeriodicOnlineOfflineRatio", 2.0);
    gearscorecheck = sConfigMgr->GetOption<bool>("AiPlayerbot.GearScoreCheck", false);
    randomBotPreQuests = sConfigMgr->GetOption<bool>("AiPlayerbot.PreQuests", false);

    // SPP automation
    freeMethodLoot = sConfigMgr->GetOption<bool>("AiPlayerbot.FreeMethodLoot", false);
    chestRollEnable = sConfigMgr->GetOption<bool>("AiPlayerbot.ChestRollEnable", false);
    rollWinGiveawayChance = sConfigMgr->GetOption<float>("AiPlayerbot.RollWinGiveawayChance", 0.0f);
    tradeAdAnchorSeconds = sConfigMgr->GetOption<uint32>("AiPlayerbot.TradeAdAnchorSeconds", 120);
    tradeDealAnchorMinSeconds = sConfigMgr->GetOption<uint32>("AiPlayerbot.TradeDealAnchorMinSeconds", 300);
    tradeDealAnchorMaxSeconds = sConfigMgr->GetOption<uint32>("AiPlayerbot.TradeDealAnchorMaxSeconds", 600);
    classServicePortalTip = sConfigMgr->GetOption<uint32>("AiPlayerbot.ClassService.PortalTip", 5000);
    classServiceSummonTip = sConfigMgr->GetOption<uint32>("AiPlayerbot.ClassService.SummonTip", 10000);
    keywordTradeReplies = sConfigMgr->GetOption<bool>("AiPlayerbot.KeywordTradeReplies", true);
    lootNeedRollLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.LootNeedRollLevel", 1);
    lootRollRecipe = sConfigMgr->GetOption<bool>("AiPlayerbot.LootRollRecipe", false);
    lootRollDisenchant = sConfigMgr->GetOption<bool>("AiPlayerbot.LootRollDisenchant", false);
    lootGreedRollLevel = sConfigMgr->GetOption<bool>("AiPlayerbot.LootGreedRollLevel", false);
    autoPickReward = sConfigMgr->GetOption<std::string>("AiPlayerbot.AutoPickReward", "yes");
    autoEquipUpgradeLoot = sConfigMgr->GetOption<bool>("AiPlayerbot.AutoEquipUpgradeLoot", true);
    equipUpgradeThreshold = sConfigMgr->GetOption<float>("AiPlayerbot.EquipUpgradeThreshold", 1.1f);
    twoRoundsGearInit = sConfigMgr->GetOption<bool>("AiPlayerbot.TwoRoundsGearInit", false);
    syncQuestWithPlayer = sConfigMgr->GetOption<bool>("AiPlayerbot.SyncQuestWithPlayer", true);
    syncQuestForPlayer = sConfigMgr->GetOption<bool>("AiPlayerbot.SyncQuestForPlayer", false);
    dropObsoleteQuests = sConfigMgr->GetOption<bool>("AiPlayerbot.DropObsoleteQuests", true);
    allowLearnTrainerSpells = sConfigMgr->GetOption<bool>("AiPlayerbot.AllowLearnTrainerSpells", true);
    autoPickTalents = sConfigMgr->GetOption<bool>("AiPlayerbot.AutoPickTalents", true);
    autoUpgradeEquip = sConfigMgr->GetOption<bool>("AiPlayerbot.AutoUpgradeEquip", true);
    hunterWolfPet = sConfigMgr->GetOption<int32>("AiPlayerbot.HunterWolfPet", 0);
    defaultPetStance = sConfigMgr->GetOption<int32>("AiPlayerbot.DefaultPetStance", 1);
    petChatCommandDebug = sConfigMgr->GetOption<bool>("AiPlayerbot.PetChatCommandDebug", 0);
    autoLearnTrainerSpells = sConfigMgr->GetOption<bool>("AiPlayerbot.AutoLearnTrainerSpells", true);
    autoLearnQuestSpells = sConfigMgr->GetOption<bool>("AiPlayerbot.AutoLearnQuestSpells", true);
    autoTeleportForLevel = sConfigMgr->GetOption<bool>("AiPlayerbot.AutoTeleportForLevel", false);
    autoDoQuests = sConfigMgr->GetOption<bool>("AiPlayerbot.AutoDoQuests", true);
    enableNewRpgStrategy = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableNewRpgStrategy", true);

    RpgStatusProbWeight[RPG_WANDER_RANDOM] = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgStatusProbWeight.WanderRandom", 15);
    RpgStatusProbWeight[RPG_WANDER_NPC] = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgStatusProbWeight.WanderNpc", 20);
    RpgStatusProbWeight[RPG_GO_GRIND] = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgStatusProbWeight.GoGrind", 15);
    RpgStatusProbWeight[RPG_GO_CAMP] = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgStatusProbWeight.GoCamp", 10);
    RpgStatusProbWeight[RPG_DO_QUEST] = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgStatusProbWeight.DoQuest", 60);
    RpgStatusProbWeight[RPG_TRAVEL_FLIGHT] = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgStatusProbWeight.TravelFlight", 15);
    RpgStatusProbWeight[RPG_REST] = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgStatusProbWeight.Rest", 5);
    RpgStatusProbWeight[RPG_OUTDOOR_PVP] = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgStatusProbWeight.OutdoorPvp", 10);
    RpgStatusProbWeight[RPG_GO_WPVP] = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgStatusProbWeight.GoWpvp", 1);
    RpgStatusProbWeight[RPG_DUEL_SPOT] = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgStatusProbWeight.DuelSpot", 1);
    RpgStatusProbWeight[RPG_GO_MOONGLADE] = sConfigMgr->GetOption<int32>("AiPlayerbot.RpgStatusProbWeight.GoMoonglade", 7);
    cityDwellChance = sConfigMgr->GetOption<float>("AiPlayerbot.CityDwellChance", 0.9f);

    // World PvP excursions (RPG_GO_WPVP)
    wpvpMinBotLevel = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpMinBotLevel", 10);
    wpvpHomeZoneChance = sConfigMgr->GetOption<float>("AiPlayerbot.WpvpHomeZoneChance", 0.15f);
    wpvpHomeZoneMinLevelGap = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpHomeZoneMinLevelGap", 10);
    wpvpHomeZoneFullChanceGap = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpHomeZoneFullChanceGap", 30);
    wpvpLowerBracketChance = sConfigMgr->GetOption<float>("AiPlayerbot.WpvpLowerBracketChance", 0.15f);
    wpvpGankerMinLevelGap = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpGankerMinLevelGap", 8);
    wpvpVisionDistance = sConfigMgr->GetOption<float>("AiPlayerbot.WpvpVisionDistance", 100.0f);
    wpvpStealthClassWeightMult = sConfigMgr->GetOption<float>("AiPlayerbot.WpvpStealthClassWeightMult", 1.5f);
    wpvpStealthClassOverlevelMult = sConfigMgr->GetOption<float>("AiPlayerbot.WpvpStealthClassOverlevelMult", 0.6f);
    wpvpTeleportOffsetMin = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpTeleportOffsetMin", 180);
    wpvpTeleportOffsetMax = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpTeleportOffsetMax", 280);
    wpvpAnchorOffsetMin = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpAnchorOffsetMin", 40);
    wpvpAnchorOffsetMax = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpAnchorOffsetMax", 80);
    wpvpDwellMinutesMin = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpDwellMinutesMin", 15);
    wpvpDwellMinutesMax = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpDwellMinutesMax", 30);
    wpvpDeathCap = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpDeathCap", 2);
    wpvpSatiationChance = std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpSatiationChance", 60), 100);
    wpvpSatiationMinutes = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpSatiationMinutes", 10);
    wpvpGrudgeMinutes = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpGrudgeMinutes", 15);
    wpvpRevengeChance = std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpRevengeChance", 60), 100);
    wpvpVendettaGanks = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpVendettaGanks", 3);
    wpvpVendettaCampGanks = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpVendettaCampGanks", 2);
    wpvpBegMercyChance = std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpBegMercyChance", 15), 100);
    wpvpChaseBreakChance = std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpChaseBreakChance", 50), 100);
    wpvpChaseBreakSecondsMin =
        std::max<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpChaseBreakSecondsMin", 12), 1);
    wpvpChaseBreakSecondsMax =
        std::max(sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpChaseBreakSecondsMax", 30), wpvpChaseBreakSecondsMin);
    wpvpPeelAdvantageYards = sConfigMgr->GetOption<float>("AiPlayerbot.WpvpPeelAdvantageYards", 25.0f);
    wpvpInitiateSelfHealth =
        std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpInitiateSelfHealth", 80), 100);
    wpvpInitiateSelfMana =
        std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpInitiateSelfMana", 60), 100);
    wpvpGoadCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpGoadCooldown", 90);
    wpvpRaidChance = std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpRaidChance", 15), 100);
    wpvpRaidBoredomSeconds = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpRaidBoredomSeconds", 120);
    wpvpRaidFlightMasters = sConfigMgr->GetOption<bool>("AiPlayerbot.WpvpRaidFlightMasters", false);
    wpvpCalloutEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.WpvpCalloutEnabled", true);
    wpvpCalloutZoneCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpCalloutZoneCooldown", 180);
    wpvpCalloutAttackerCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpCalloutAttackerCooldown", 600);
    // Whether playerbots itself SAYS the prebaked defense-callout lines. The
    // callout system (board, notifications, escalation tracking) runs either
    // way; mod-llm listens to the notifications and can supply the words.
    wpvpCallouts = sConfigMgr->GetOption<bool>("AiPlayerbot.WpvpCallouts", true);
    wpvpDefenseEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.WpvpDefenseEnabled", true);
    wpvpDefenseResponseChance = sConfigMgr->GetOption<float>("AiPlayerbot.WpvpDefenseResponseChance", 10.0f);
    wpvpDefenseEvenFightChance = sConfigMgr->GetOption<float>("AiPlayerbot.WpvpDefenseEvenFightChance", 3.0f);
    wpvpGankLevelGap = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpGankLevelGap", 5);
    wpvpGuardRespectLevelGap = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpGuardRespectLevelGap", 5);
    wpvpDefenseDelayFactor = sConfigMgr->GetOption<float>("AiPlayerbot.WpvpDefenseDelayFactor", 0.5f);
    wpvpDefenseDwellMinutesMin = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpDefenseDwellMinutesMin", 3);
    wpvpDefenseDwellMinutesMax = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpDefenseDwellMinutesMax", 8);
    wpvpDefenseLevelSlack = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpDefenseLevelSlack", 3);
    wpvpDefenseResponderCap = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpDefenseResponderCap", 3);
    wpvpResponseLevelMargin = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpResponseLevelMargin", 5);
    wpvpNpcAttackDefense = sConfigMgr->GetOption<bool>("AiPlayerbot.WpvpNpcAttackDefenseEnabled", true);
    wpvpEscalationKills = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpEscalationKills", 3);
    wpvpEscalationWindow = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpEscalationWindow", 600);
    wpvpReinforcementEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.WpvpReinforcementEnabled", true);
    wpvpReinforcementDeaths = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpReinforcementDeaths", 2);
    wpvpReinforcementChance = sConfigMgr->GetOption<float>("AiPlayerbot.WpvpReinforcementChance", 15.0f);
    wpvpReinforcementCap = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpReinforcementCap", 3);
    wpvpReinforcementEscalationDeaths =
        sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpReinforcementEscalationDeaths", 2);
    wpvpEmoteAlertEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.WpvpEmoteAlertEnabled", true);
    wpvpPasserbyAssistEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.WpvpPasserbyAssistEnabled", true);
    wpvpPasserbyAssistChance =
        std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpPasserbyAssistChance", 90), 100);
    wpvpPasserbyAssistRadius = sConfigMgr->GetOption<float>("AiPlayerbot.WpvpPasserbyAssistRadius", 40.0f);
    bgStrategyComplianceChance = sConfigMgr->GetOption<uint32>("AiPlayerbot.BgStrategyComplianceChance", 65);
    bgStrategyOrderDuration = sConfigMgr->GetOption<uint32>("AiPlayerbot.BgStrategyOrderDuration", 45);
    wpvpClassTruceChance.fill(0);
    {
        static std::unordered_map<std::string, uint8> const classesByName = {
            {"warrior", CLASS_WARRIOR}, {"paladin", CLASS_PALADIN}, {"hunter", CLASS_HUNTER},
            {"rogue", CLASS_ROGUE},     {"priest", CLASS_PRIEST},   {"deathknight", CLASS_DEATH_KNIGHT},
            {"shaman", CLASS_SHAMAN},   {"mage", CLASS_MAGE},       {"warlock", CLASS_WARLOCK},
            {"druid", CLASS_DRUID}};

        std::string const truceSpec =
            sConfigMgr->GetOption<std::string>("AiPlayerbot.WpvpClassTruceChance", "druid:60,hunter:25");
        for (std::string const& token : split(truceSpec, ','))
        {
            std::string entry = token;
            entry.erase(std::remove_if(entry.begin(), entry.end(), ::isspace), entry.end());
            if (entry.empty())
                continue;

            size_t const colon = entry.find(':');
            auto klass = colon == std::string::npos ? classesByName.end()
                                                    : classesByName.find(entry.substr(0, colon));
            int32 const chance = colon == std::string::npos ? -1 : atoi(entry.c_str() + colon + 1);
            if (klass == classesByName.end() || chance < 0 || chance > 100)
            {
                LOG_ERROR("playerbots", "AiPlayerbot.WpvpClassTruceChance: bad entry '{}' (want class:percent)",
                          token);
                continue;
            }

            wpvpClassTruceChance[klass->second] = uint32(chance);
        }
    }
    wpvpTruceOathbreakerChance =
        std::min<uint32>(sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpTruceOathbreakerChance", 15), 100);
    wpvpKillSwitchDefaultMinutes = sConfigMgr->GetOption<uint32>("AiPlayerbot.WpvpKillSwitchDefaultMinutes", 60);

    enableBotDuels = sConfigMgr->GetOption<bool>("AiPlayerbot.EnableBotDuels", true);
    duelChatter = sConfigMgr->GetOption<bool>("AiPlayerbot.DuelChatter", true);
    duelChallengeRange = sConfigMgr->GetOption<uint32>("AiPlayerbot.DuelChallengeRange", 25);
    duelChallengeCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.DuelChallengeCooldown", 600);
    duelSpotChallengeCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.DuelSpotChallengeCooldown", 60);
    duelSpotSolicitCooldown = sConfigMgr->GetOption<uint32>("AiPlayerbot.DuelSpotSolicitCooldown", 120);
    duelSpotMinBotLevel = sConfigMgr->GetOption<uint32>("AiPlayerbot.DuelSpotMinBotLevel", 5);
    duelSpotDwellMinutesMin = sConfigMgr->GetOption<uint32>("AiPlayerbot.DuelSpotDwellMinutesMin", 10);
    duelSpotDwellMinutesMax = sConfigMgr->GetOption<uint32>("AiPlayerbot.DuelSpotDwellMinutesMax", 25);
    duelConsumables = sConfigMgr->GetOption<uint32>("AiPlayerbot.DuelConsumables",
                                                    static_cast<uint32>(DuelConsumables::ALL));

    syncLevelWithPlayers = sConfigMgr->GetOption<bool>("AiPlayerbot.SyncLevelWithPlayers", false);
    randomBotGroupNearby = sConfigMgr->GetOption<bool>("AiPlayerbot.RandomBotGroupNearby", false);

    // arena
    randomBotArenaTeam2v2Count = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotArenaTeam2v2Count", 10);
    randomBotArenaTeam3v3Count = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotArenaTeam3v3Count", 10);
    randomBotArenaTeam5v5Count = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotArenaTeam5v5Count", 5);
    deleteRandomBotArenaTeams = sConfigMgr->GetOption<bool>("AiPlayerbot.DeleteRandomBotArenaTeams", false);
    randomBotArenaTeamMaxRating = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotArenaTeamMaxRating", 2000);
    randomBotArenaTeamMinRating = sConfigMgr->GetOption<int32>("AiPlayerbot.RandomBotArenaTeamMinRating", 1000);

    selfBotLevel = sConfigMgr->GetOption<int32>("AiPlayerbot.SelfBotLevel", 1);

    RandomPlayerbotFactory::CreateRandomBots();
    if (World::IsStopped())
    {
        return true;
    }

    // Assign account types after accounts are created
    sRandomPlayerbotMgr.AssignAccountTypes();

    if (sPlayerbotAIConfig.enabled)
    {
        sRandomPlayerbotMgr.Init();
    }

    PlayerbotGuildMgr::instance().Init();
    sRandomItemMgr.Init();
    sRandomItemMgr.InitAfterAhBot();
    sBisListMgr->LoadAll();
    PlayerbotTextMgr::instance().LoadBotTexts();
    PlayerbotTextMgr::instance().LoadBotTextChance();
    PlayerbotFactory::Init();

    AiObjectContext::BuildAllSharedContexts();

    if (sPlayerbotAIConfig.randomBotSuggestDungeons)
    {
        PlayerbotDungeonRepository::instance().LoadDungeonSuggestions();
    }
    sTravelMgr.Init();

    excludedHunterPetFamilies.clear();
    LoadList<std::vector<uint32>>(sConfigMgr->GetOption<std::string>("AiPlayerbot.ExcludedHunterPetFamilies", ""), excludedHunterPetFamilies);

    LOG_INFO("server.loading", "---------------------------------------");
    LOG_INFO("server.loading", "       mod-playerbots initialized      ");
    LOG_INFO("server.loading", "---------------------------------------");

    return true;
}

// Loads AiPlayerbot.LevelBrackets.* and AiPlayerbot.ResetBotLevel.* (see RandomBotLevelMgr). Also
// re-run on ".reload config" via RandomBotLevelWorldScript::OnAfterConfigLoad. Bracket
// bounds/percentages are only the as-configured values here; RandomBotLevelMgr::LoadConfig()
// copies them into its own working state, since dynamic distribution and clamp/rebalance mutate
// percentages at runtime.
void PlayerbotAIConfig::LoadRandomBotLevelConfig()
{
    // ---- Level brackets ----
    levelBracketsEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.LevelBrackets.Enabled", false);
    levelBracketsIgnoreGuildWithRealPlayers =
        sConfigMgr->GetOption<bool>("AiPlayerbot.LevelBrackets.IgnoreGuildBotsWithRealPlayers", true);
    levelBracketsIgnoreArenaTeamBots =
        sConfigMgr->GetOption<bool>("AiPlayerbot.LevelBrackets.IgnoreArenaTeamBots", true);

    levelBracketsCheckFrequency = sConfigMgr->GetOption<uint32>("AiPlayerbot.LevelBrackets.CheckFrequency", 300);
    levelBracketsFlaggedCheckFrequency =
        sConfigMgr->GetOption<uint32>("AiPlayerbot.LevelBrackets.CheckFlaggedFrequency", 15);
    levelBracketsDynamicDistribution =
        sConfigMgr->GetOption<bool>("AiPlayerbot.LevelBrackets.Dynamic.UseDynamicDistribution", false);
    levelBracketsRealPlayerWeight =
        sConfigMgr->GetOption<float>("AiPlayerbot.LevelBrackets.Dynamic.RealPlayerWeight", 1.0f);
    levelBracketsSyncFactions = sConfigMgr->GetOption<bool>("AiPlayerbot.LevelBrackets.Dynamic.SyncFactions", false);
    levelBracketsIgnoreFriendListed = sConfigMgr->GetOption<bool>("AiPlayerbot.LevelBrackets.IgnoreFriendListed", true);
    levelBracketsFlaggedProcessLimit =
        sConfigMgr->GetOption<uint32>("AiPlayerbot.LevelBrackets.FlaggedProcessLimit", 5);

    ParseLevelMgrExcludeNames(sConfigMgr->GetOption<std::string>("AiPlayerbot.LevelBrackets.ExcludeNames", ""),
        levelBracketsExcludeNames);

    levelBracketsNumRanges =
        static_cast<uint8>(sConfigMgr->GetOption<uint32>("AiPlayerbot.LevelBrackets.NumRanges", 9));
    levelBracketsAlliance.resize(levelBracketsNumRanges);
    levelBracketsHorde.resize(levelBracketsNumRanges);

    for (uint8 i = 0; i < levelBracketsNumRanges; ++i)
    {
        std::string idx = std::to_string(i + 1);
        uint32 defaultLower = (i == 0 ? 1 : i * 10);
        uint32 defaultUpper = (i < levelBracketsNumRanges - 1 ? i * 10 + 9 : randomBotMaxLevel);
        levelBracketsAlliance[i].lower = static_cast<uint8>(
            sConfigMgr->GetOption<uint32>("AiPlayerbot.LevelBrackets.Alliance.Range" + idx + ".Lower", defaultLower));
        levelBracketsAlliance[i].upper = static_cast<uint8>(
            sConfigMgr->GetOption<uint32>("AiPlayerbot.LevelBrackets.Alliance.Range" + idx + ".Upper", defaultUpper));
        levelBracketsAlliance[i].pct = static_cast<uint8>(
            sConfigMgr->GetOption<uint32>("AiPlayerbot.LevelBrackets.Alliance.Range" + idx + ".Pct", 11));
    }

    for (uint8 i = 0; i < levelBracketsNumRanges; ++i)
    {
        std::string idx = std::to_string(i + 1);
        uint32 defaultLower = (i == 0 ? 1 : i * 10);
        uint32 defaultUpper = (i < levelBracketsNumRanges - 1 ? i * 10 + 9 : randomBotMaxLevel);
        levelBracketsHorde[i].lower = static_cast<uint8>(
            sConfigMgr->GetOption<uint32>("AiPlayerbot.LevelBrackets.Horde.Range" + idx + ".Lower", defaultLower));
        levelBracketsHorde[i].upper = static_cast<uint8>(
            sConfigMgr->GetOption<uint32>("AiPlayerbot.LevelBrackets.Horde.Range" + idx + ".Upper", defaultUpper));
        levelBracketsHorde[i].pct = static_cast<uint8>(
            sConfigMgr->GetOption<uint32>("AiPlayerbot.LevelBrackets.Horde.Range" + idx + ".Pct", 11));
    }

    // A mismatch forcibly disables SyncFactions and logs an error; it never brings the server down.
    if (levelBracketsSyncFactions)
    {
        for (uint8 i = 0; i < levelBracketsNumRanges; ++i)
        {
            if (levelBracketsAlliance[i].lower != levelBracketsHorde[i].lower ||
                levelBracketsAlliance[i].upper != levelBracketsHorde[i].upper)
            {
                LOG_ERROR("server.loading",
                    "[RandomBotLevelMgr] Bracket mismatch detected between factions at index {}. Alliance: {}-{}, "
                    "Horde: {}-{}. SyncFactions requires both bracket count and min/max levels to match exactly; "
                    "forcibly disabling SyncFactions for this session. Check your configuration.",
                    i, levelBracketsAlliance[i].lower, levelBracketsAlliance[i].upper,
                    levelBracketsHorde[i].lower, levelBracketsHorde[i].upper);
                levelBracketsSyncFactions = false;
                break;
            }
        }
    }

    // ---- Level reset ----
    resetBotLevelEnabled = sConfigMgr->GetOption<bool>("AiPlayerbot.ResetBotLevel.Enabled", false);

    resetBotLevelMaxLevel =
        static_cast<uint8>(sConfigMgr->GetOption<uint32>("AiPlayerbot.ResetBotLevel.MaxLevel", 80));
    if ((resetBotLevelMaxLevel < 2 || resetBotLevelMaxLevel > 80) && resetBotLevelMaxLevel != 0)
    {
        LOG_ERROR("server.loading",
            "[RandomBotLevelMgr] Invalid AiPlayerbot.ResetBotLevel.MaxLevel value: {}. Using default value 80.",
            resetBotLevelMaxLevel);
        resetBotLevelMaxLevel = 80;
    }

    resetBotLevelResetTo =
        static_cast<uint8>(sConfigMgr->GetOption<uint32>("AiPlayerbot.ResetBotLevel.ResetToLevel", 1));
    if (resetBotLevelResetTo < 1 || (resetBotLevelMaxLevel > 0 && resetBotLevelResetTo >= resetBotLevelMaxLevel))
    {
        LOG_ERROR("server.loading",
            "[RandomBotLevelMgr] Invalid AiPlayerbot.ResetBotLevel.ResetToLevel value: {}. Using default value 1.",
            resetBotLevelResetTo);
        resetBotLevelResetTo = 1;
    }

    resetBotLevelSkipFrom =
        static_cast<uint8>(sConfigMgr->GetOption<uint32>("AiPlayerbot.ResetBotLevel.SkipFromLevel", 0));
    if (resetBotLevelSkipFrom > 80 || (resetBotLevelMaxLevel > 0 && resetBotLevelSkipFrom >= resetBotLevelMaxLevel))
    {
        LOG_ERROR("server.loading",
            "[RandomBotLevelMgr] Invalid AiPlayerbot.ResetBotLevel.SkipFromLevel value: {}. Using default value 0 "
            "(disabled).",
            resetBotLevelSkipFrom);
        resetBotLevelSkipFrom = 0;
    }

    resetBotLevelSkipTo = static_cast<uint8>(sConfigMgr->GetOption<uint32>("AiPlayerbot.ResetBotLevel.SkipToLevel", 1));
    if (resetBotLevelSkipTo < 1 || resetBotLevelSkipTo > 80 ||
        (resetBotLevelMaxLevel > 0 && resetBotLevelSkipTo > resetBotLevelMaxLevel))
    {
        LOG_ERROR("server.loading",
            "[RandomBotLevelMgr] Invalid AiPlayerbot.ResetBotLevel.SkipToLevel value: {}. Using default value 1.",
            resetBotLevelSkipTo);
        resetBotLevelSkipTo = 1;
    }

    resetBotLevelChance =
        static_cast<uint8>(sConfigMgr->GetOption<uint32>("AiPlayerbot.ResetBotLevel.ResetChance", 100));
    if (resetBotLevelChance > 100)
    {
        LOG_ERROR("server.loading",
            "[RandomBotLevelMgr] Invalid AiPlayerbot.ResetBotLevel.ResetChance value: {}. Using default value 100.",
            resetBotLevelChance);
        resetBotLevelChance = 100;
    }

    resetBotLevelScaledChance = sConfigMgr->GetOption<bool>("AiPlayerbot.ResetBotLevel.ScaledChance", false);

    resetBotLevelRestrictTimePlayed =
        sConfigMgr->GetOption<bool>("AiPlayerbot.ResetBotLevel.RestrictTimePlayed", false);
    resetBotLevelMinTimePlayed = sConfigMgr->GetOption<uint32>("AiPlayerbot.ResetBotLevel.MinTimePlayed", 86400);
    resetBotLevelPlayedTimeCheckFrequency =
        sConfigMgr->GetOption<uint32>("AiPlayerbot.ResetBotLevel.PlayedTimeCheckFrequency", 864);

    resetBotLevelIgnoreGuildWithRealPlayers =
        sConfigMgr->GetOption<bool>("AiPlayerbot.ResetBotLevel.IgnoreGuildBotsWithRealPlayers", false);

    ParseLevelMgrExcludeNames(sConfigMgr->GetOption<std::string>("AiPlayerbot.ResetBotLevel.ExcludeNames", ""),
        resetBotLevelExcludeNames);
}

bool PlayerbotAIConfig::IsInRandomAccountList(uint32 id)
{
    return find(randomBotAccounts.begin(), randomBotAccounts.end(), id) != randomBotAccounts.end();
}

bool PlayerbotAIConfig::IsInRandomQuestItemList(uint32 id)
{
    return find(randomBotQuestItems.begin(), randomBotQuestItems.end(), id) != randomBotQuestItems.end();
}

bool PlayerbotAIConfig::IsPvpProhibited(uint32 zoneId, uint32 areaId)
{
    return IsInPvpProhibitedZone(zoneId) || IsInPvpProhibitedArea(areaId) || IsInPvpProhibitedZone(areaId);
}

bool PlayerbotAIConfig::IsInPvpProhibitedZone(uint32 id)
{
    return find(pvpProhibitedZoneIds.begin(), pvpProhibitedZoneIds.end(), id) != pvpProhibitedZoneIds.end();
}

bool PlayerbotAIConfig::IsInPvpProhibitedArea(uint32 id)
{
    return find(pvpProhibitedAreaIds.begin(), pvpProhibitedAreaIds.end(), id) != pvpProhibitedAreaIds.end();
}

bool PlayerbotAIConfig::IsRestrictedHealerDPSMap(uint32 mapId) const
{
    return restrictHealerDPS &&
            std::find(restrictedHealerDPSMaps.begin(), restrictedHealerDPSMaps.end(), mapId) != restrictedHealerDPSMaps.end();
}

std::string const PlayerbotAIConfig::GetTimestampStr()
{
    time_t t = time(nullptr);
    tm* aTm = localtime(&t);
    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d-%02d-%02d", aTm->tm_year + 1900, aTm->tm_mon + 1, aTm->tm_mday, aTm->tm_hour,
             aTm->tm_min, aTm->tm_sec);
    return std::string(buf);
}

bool PlayerbotAIConfig::openLog(std::string const fileName, char const* mode)
{
    if (!hasLog(fileName))
        return false;

    auto logFileIt = logFiles.find(fileName);
    if (logFileIt == logFiles.end())
    {
        logFiles.insert(std::make_pair(fileName, std::make_pair(nullptr, false)));
        logFileIt = logFiles.find(fileName);
    }

    FILE* file = logFileIt->second.first;
    bool fileOpen = logFileIt->second.second;

    if (fileOpen)  // close log file
        fclose(file);

    std::string m_logsDir = sConfigMgr->GetOption<std::string>("LogsDir", "", false);
    if (!m_logsDir.empty())
    {
        if ((m_logsDir.at(m_logsDir.length() - 1) != '/') && (m_logsDir.at(m_logsDir.length() - 1) != '\\'))
            m_logsDir.append("/");
    }

    file = fopen((m_logsDir + fileName).c_str(), mode);
    fileOpen = true;

    logFileIt->second.first = file;
    logFileIt->second.second = fileOpen;

    return true;
}

void PlayerbotAIConfig::log(std::string const fileName, char const* str, ...)
{
    if (!str)
        return;

    std::lock_guard<std::mutex> guard(m_logMtx);

    if (!isLogOpen(fileName) && !openLog(fileName, "a"))
        return;

    FILE* file = logFiles.find(fileName)->second.first;

    va_list ap;
    va_start(ap, str);
    vfprintf(file, str, ap);
    fprintf(file, "\n");
    va_end(ap);
    fflush(file);

    fflush(stdout);
}

void PlayerbotAIConfig::loadWorldBuff()
{
    std::string matrix = sConfigMgr->GetOption<std::string>("AiPlayerbot.WorldBuffMatrix", "", true);
    if (matrix.empty())
        return;

    std::istringstream entryStream(matrix);
    std::string entry;

    while (std::getline(entryStream, entry, ';'))
    {

        entry.erase(0, entry.find_first_not_of(" \t\r\n"));
        entry.erase(entry.find_last_not_of(" \t\r\n") + 1);

        size_t firstColon = entry.find(':');
        size_t secondColon = entry.find(':', firstColon + 1);

        if (firstColon == std::string::npos || secondColon == std::string::npos)
        {
            LOG_ERROR("playerbots", "Malformed entry: [{}]", entry);
            continue;
        }

        std::string metaPart = entry.substr(firstColon + 1, secondColon - firstColon - 1);
        std::string spellPart = entry.substr(secondColon + 1);

        std::vector<uint32> ids;
        std::istringstream metaStream(metaPart);
        std::string token;
        while (std::getline(metaStream, token, ','))
        {
            try {
                ids.push_back(static_cast<uint32>(std::stoi(token)));
            } catch (...) {
                LOG_ERROR("playerbots", "Invalid meta token in [{}]", entry);
                break;
            }
        }

        if (ids.size() != 5)
        {
            LOG_ERROR("playerbots", "Entry [{}] has incomplete meta block", entry);
            continue;
        }

        std::istringstream spellStream(spellPart);
        while (std::getline(spellStream, token, ','))
        {
            try {
                uint32 spellId = static_cast<uint32>(std::stoi(token));
                worldBuff wb = { spellId, ids[0], ids[1], ids[2], ids[3], ids[4] };
                worldBuffs.push_back(wb);
            } catch (...) {
                LOG_ERROR("playerbots", "Invalid spell ID in [{}]", entry);
            }
        }
    }
}

static std::vector<std::string> split(const std::string& str, const std::string& pattern)
{
    std::vector<std::string> res;
    if (str == "")
        return res;
    // Also add separators to string connections to facilitate intercepting the last paragraph.
    std::string strs = str + pattern;
    size_t pos = strs.find(pattern);

    while (pos != strs.npos)
    {
        std::string temp = strs.substr(0, pos);
        res.push_back(temp);
        // Remove the split string and split the remaining string
        strs = strs.substr(pos + 1, strs.size());
        pos = strs.find(pattern);
    }

    return res;
}

std::vector<std::vector<uint32>> PlayerbotAIConfig::ParseTempTalentsOrder(uint32 cls, std::string tab_link)
{
    // check bad link
    uint32 classMask = 1 << (cls - 1);
    std::vector<std::vector<uint32>> res;
    std::vector<std::string> tab_links = split(tab_link, "-");
    std::map<uint32, std::vector<TalentEntry const*>> spells;
    std::vector<std::vector<std::vector<uint32>>> orders(3);
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
            continue;

        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
        if (!talentTabInfo)
            continue;

        if ((classMask & talentTabInfo->ClassMask) == 0)
            continue;

        spells[talentTabInfo->tabpage].push_back(talentInfo);
    }
    for (int tab = 0; tab < 3; tab++)
    {
        if (tab_links.size() <= (size_t)tab)
        {
            break;
        }
        std::sort(spells[tab].begin(), spells[tab].end(),
                  [&](TalentEntry const* lhs, TalentEntry const* rhs)
                  { return lhs->Row != rhs->Row ? lhs->Row < rhs->Row : lhs->Col < rhs->Col; });
        for (uint32 i = 0; i < tab_links[tab].size(); i++)
        {
            if (i >= spells[tab].size())
            {
                break;
            }
            int lvl = tab_links[tab][i] - '0';
            if (lvl == 0)
                continue;
            orders[tab].push_back({(uint32)tab, spells[tab][i]->Row, spells[tab][i]->Col, (uint32)lvl});
        }
    }
    // sort by talent tab size
    std::sort(orders.begin(), orders.end(), [&](auto& lhs, auto& rhs) { return lhs.size() > rhs.size(); });
    for (auto& order : orders)
    {
        res.insert(res.end(), order.begin(), order.end());
    }
    return res;
}

std::vector<std::vector<uint32>> PlayerbotAIConfig::ParseTempPetTalentsOrder(uint32 spec, std::string tab_link)
{
    // check bad link
    // uint32 classMask = 1 << (cls - 1);
    std::vector<TalentEntry const*> spells;
    std::vector<std::vector<uint32>> orders;
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
            continue;

        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
        if (!talentTabInfo)
            continue;

        if (!((1 << spec) & talentTabInfo->petTalentMask))
            continue;
        // skip some duplicate spells like dash/dive
        if (talentInfo->TalentID == 2201 || talentInfo->TalentID == 2208 || talentInfo->TalentID == 2219 ||
            talentInfo->TalentID == 2203)
            continue;

        spells.push_back(talentInfo);
    }
    std::sort(spells.begin(), spells.end(),
              [&](TalentEntry const* lhs, TalentEntry const* rhs)
              { return lhs->Row != rhs->Row ? lhs->Row < rhs->Row : lhs->Col < rhs->Col; });
    for (uint32 i = 0; i < tab_link.size(); i++)
    {
        if (i >= spells.size())
        {
            break;
        }
        int lvl = tab_link[i] - '0';
        if (lvl == 0)
            continue;
        orders.push_back({spells[i]->Row, spells[i]->Col, (uint32)lvl});
    }
    // sort by talent tab size
    std::sort(orders.begin(), orders.end(), [&](auto& lhs, auto& rhs) { return lhs.size() > rhs.size(); });

    return orders;
}
