#include "WpvpGrudge.h"

#include <unordered_set>
#include <vector>

#include "CombatManager.h"
#include "DeterministicRoll.h"
#include "EmoteAction.h"
#include "FelworldEvents.h"
#include "LevelPerception.h"
#include "Metric.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Random.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include "Timer.h"
#include "World.h"
#include "WpvpChase.h"

namespace
{
// Revenge wants a winnable rematch: a killer who reads this far above the
// bot never inspires one (mirrors the courage gates' EXTREME_LEVEL_DIFF -
// revenge bypasses those dice, so the suicide line is enforced here).
constexpr int32 REVENGE_OUTCLASS_GAP = 5;

// How close the killer gets before an avoidant bot reacts, and how far each
// retreat leg runs. The notice range matches the passerby-assist scale - the
// killer being "on the bot's screen" - and the step is long enough that one
// or two legs put the bot back outside it.
constexpr float AVOID_NOTICE_RANGE = 40.0f;
constexpr float AVOID_RETREAT_STEP = 30.0f;

// One plea per grudge per this long - begging is a beat, not a soundtrack.
constexpr uint32 PLEA_COOLDOWN_MS = 20 * IN_MILLISECONDS;

// Mercy dice hold for a window so emote spam doesn't reroll them.
constexpr uint64 MERCY_ROLL_SALT = 0x4D524359;  // 'MRCY'
constexpr uint32 MERCY_ROLL_WINDOW = 2 * MINUTE;

bool IsPleaEmote(uint32 textEmote)
{
    return textEmote == TEXT_EMOTE_BEG || textEmote == TEXT_EMOTE_CRY || textEmote == TEXT_EMOTE_SHOO;
}
}

WpvpGrudgeDisposition WpvpGrudgeAgainst(Player* bot, Player* enemy)
{
    if (!sPlayerbotAIConfig.wpvpGrudgeMinutes || !bot || !enemy)
        return WpvpGrudgeDisposition::None;

    return WpvpGrudgeBoard::instance().Disposition(bot->GetGUID(), enemy->GetGUID());
}

Player* WpvpAvoidantKillerNear(Player* bot, float range)
{
    if (!sPlayerbotAIConfig.wpvpGrudgeMinutes)
        return nullptr;

    Player* nearest = nullptr;
    for (ObjectGuid const& guid : WpvpGrudgeBoard::instance().AvoidantKillers(bot->GetGUID()))
    {
        Player* killer = ObjectAccessor::GetPlayer(*bot, guid);
        if (!killer || !killer->IsAlive() || !bot->CanSeeOrDetect(killer))
            continue;

        if (bot->GetDistance(killer) > range)
            continue;

        // Already trading blows: dodging is over, self-defense owns the bot.
        if (bot->GetCombatManager().GetPvPCombatRefs().count(guid))
            continue;

        if (!nearest || bot->GetDistance(killer) < bot->GetDistance(nearest))
            nearest = killer;
    }

    return nearest;
}

void NoteMercyPlea(Player* emoter, uint32 textEmote, ObjectGuid targetGuid)
{
    if (!sPlayerbotAIConfig.wpvpBegMercyChance || !IsPleaEmote(textEmote))
        return;

    if (!emoter || emoter->InBattleground() || emoter->InArena())
        return;

    // Everyone currently on the emoter, plus whoever the plea was aimed at -
    // an aimed plea can preempt a stalker who hasn't swung yet.
    std::vector<Player*> candidates;
    for (auto const& [guid, combatRef] : emoter->GetCombatManager().GetPvPCombatRefs())
    {
        Unit* other = combatRef->GetOther(emoter);
        if (other && other->IsPlayer())
            candidates.push_back(other->ToPlayer());
    }

    if (targetGuid.IsPlayer() && targetGuid != emoter->GetGUID())
        if (Player* target = ObjectAccessor::GetPlayer(*emoter, targetGuid))
            candidates.push_back(target);

    float const listenRange = sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_TEXTEMOTE);

    std::unordered_set<ObjectGuid> seen;
    for (Player* attacker : candidates)
    {
        if (!seen.insert(attacker->GetGUID()).second)
            continue;

        // Only bots are moved to mercy - a real player's heart is their own.
        if (attacker->GetTeamId() == emoter->GetTeamId() || !GET_PLAYERBOT_AI(attacker))
            continue;

        // The plea has to be heard: same radius real players see the emote in.
        if (!emoter->IsWithinDist(attacker, listenRange))
            continue;

        // An untargeted plea only moves someone actually on the emoter.
        bool const aggressor = attacker->GetVictim() == emoter ||
                               emoter->GetCombatManager().GetPvPCombatRefs().count(attacker->GetGUID());
        if (!aggressor && attacker->GetGUID() != targetGuid)
            continue;

        bool const moved =
            DeterministicRollPasses(attacker->GetGUID().GetRawValue(), emoter->GetGUID().GetRawValue(),
                                    MERCY_ROLL_SALT, MERCY_ROLL_WINDOW, sPlayerbotAIConfig.wpvpBegMercyChance);
        METRIC_VALUE("playerbots_wpvp_mercy", 1, METRIC_TAG("outcome", moved ? "moved" : "unmoved"));
        if (!moved)
            continue;

        LOG_DEBUG("playerbots", "Bot {} is moved to mercy by {}'s plea", attacker->GetName(), emoter->GetName());
        Felworld::LogEvent(attacker->GetGUID(), "wpvp_mercy",
                           Acore::StringFormat("{{\"beggar\":\"{}\"}}", emoter->GetName()));
        WpvpChaseBoard::instance().GrantMercy(attacker, emoter);
    }
}

void WpvpGrudgeBoard::RecordKill(Player* killer, Player* victim)
{
    uint32 const now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);
    Prune(now);

    // Revenge achieved: whatever grudge the killer held against this victim
    // is settled - the book closes instead of opening a fresh page.
    if (auto held = _grudges.find(killer->GetGUID()); held != _grudges.end())
    {
        if (held->second.erase(victim->GetGUID()))
        {
            LOG_DEBUG("playerbots", "Bot {} settles its grudge against {}", killer->GetName(), victim->GetName());
            Felworld::LogEvent(killer->GetGUID(), "wpvp_grudge_settled",
                               Acore::StringFormat("{{\"victim\":\"{}\"}}", victim->GetName()));
        }

        if (held->second.empty())
            _grudges.erase(held);
    }

    if (!sPlayerbotAIConfig.wpvpGrudgeMinutes)
        return;

    // Only bots hold grudges - a real player's memory is their own.
    if (!GET_PLAYERBOT_AI(victim))
        return;

    Grudge& grudge = _grudges[victim->GetGUID()][killer->GetGUID()];
    grudge.deaths++;
    grudge.untilMs = now + sPlayerbotAIConfig.wpvpGrudgeMinutes * MINUTE * IN_MILLISECONDS;

    // Only the first death to this killer inspires revenge - the second
    // teaches a lesson - and a killer who plainly outclasses the bot never
    // does.
    bool const revenge = grudge.deaths == 1 &&
                         int32(PerceivedLevel(victim, killer)) - int32(victim->GetLevel()) < REVENGE_OUTCLASS_GAP &&
                         urand(1, 100) <= sPlayerbotAIConfig.wpvpRevengeChance;
    grudge.disposition = revenge ? WpvpGrudgeDisposition::Revenge : WpvpGrudgeDisposition::Avoidant;

    METRIC_VALUE("playerbots_wpvp_grudge", 1, METRIC_TAG("disposition", revenge ? "revenge" : "avoidant"));
    LOG_DEBUG("playerbots", "Bot {} holds a{} grudge against {} (death {})", victim->GetName(),
              revenge ? " vengeful" : "n avoidant", killer->GetName(), grudge.deaths);
    Felworld::LogEvent(victim->GetGUID(), "wpvp_grudge",
                       Acore::StringFormat("{{\"killer\":\"{}\",\"disposition\":\"{}\",\"deaths\":{}}}",
                                           killer->GetName(), revenge ? "revenge" : "avoidant", grudge.deaths));
}

WpvpGrudgeDisposition WpvpGrudgeBoard::Disposition(ObjectGuid victim, ObjectGuid killer)
{
    uint32 const now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);

    auto held = _grudges.find(victim);
    if (held == _grudges.end())
        return WpvpGrudgeDisposition::None;

    auto it = held->second.find(killer);
    if (it == held->second.end() || int32(it->second.untilMs - now) <= 0)
        return WpvpGrudgeDisposition::None;

    return it->second.disposition;
}

std::vector<ObjectGuid> WpvpGrudgeBoard::AvoidantKillers(ObjectGuid victim)
{
    uint32 const now = getMSTime();

    std::vector<ObjectGuid> killers;

    std::lock_guard<std::mutex> lock(_mutex);

    auto held = _grudges.find(victim);
    if (held == _grudges.end())
        return killers;

    for (auto const& [killer, grudge] : held->second)
        if (grudge.disposition == WpvpGrudgeDisposition::Avoidant && int32(grudge.untilMs - now) > 0)
            killers.push_back(killer);

    return killers;
}

bool WpvpGrudgeBoard::ClaimPleaEmote(ObjectGuid victim, ObjectGuid killer)
{
    uint32 const now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);

    auto held = _grudges.find(victim);
    if (held == _grudges.end())
        return false;

    auto it = held->second.find(killer);
    if (it == held->second.end() || it->second.disposition != WpvpGrudgeDisposition::Avoidant ||
        int32(it->second.untilMs - now) <= 0)
        return false;

    if (it->second.nextPleaMs && int32(it->second.nextPleaMs - now) > 0)
        return false;

    it->second.nextPleaMs = now + PLEA_COOLDOWN_MS;
    return true;
}

void WpvpGrudgeBoard::Prune(uint32 now)
{
    for (auto held = _grudges.begin(); held != _grudges.end();)
    {
        for (auto it = held->second.begin(); it != held->second.end();)
        {
            if (int32(it->second.untilMs - now) <= 0)
                it = held->second.erase(it);
            else
                ++it;
        }

        if (held->second.empty())
            held = _grudges.erase(held);
        else
            ++held;
    }
}

bool WpvpAvoidKillerTrigger::IsActive()
{
    if (!bot->IsAlive() || bot->InBattleground() || bot->InArena())
        return false;

    return WpvpAvoidantKillerNear(bot, AVOID_NOTICE_RANGE) != nullptr;
}

bool WpvpAvoidKillerAction::Execute(Event /*event*/)
{
    Player* killer = WpvpAvoidantKillerNear(bot, AVOID_NOTICE_RANGE);
    if (!killer)
        return false;

    // The plea first: face them and wave the fight off. Not just theater -
    // the text-emote hook may move a bot killer to mercy (NoteMercyPlea).
    if (WpvpGrudgeBoard::instance().ClaimPleaEmote(bot->GetGUID(), killer->GetGUID()))
    {
        static constexpr TextEmotes PLEAS[] = {TEXT_EMOTE_SHOO, TEXT_EMOTE_BEG, TEXT_EMOTE_CRY};
        TextEmotes const plea = PLEAS[urand(0, 2)];

        bot->SetFacingToObject(killer);

        // Targeted text emote so "<name> begs you" lands in the killer's
        // chat log, same delivery as the truce salute.
        ObjectGuid const oldSelection = bot->GetTarget();
        bot->SetSelection(killer->GetGUID());

        WorldPacket data(SMSG_TEXT_EMOTE);
        data << uint32(plea);
        data << EmoteActionBase::GetNumberOfEmoteVariants(plea, bot->getRace(), bot->getGender());
        data << killer->GetGUID();
        bot->GetSession()->HandleTextEmoteOpcode(data);

        bot->SetSelection(oldSelection);
    }

    // Then leg it. The trigger keeps firing while the killer stays inside
    // notice range, so a killer who keeps closing keeps being fled.
    return MoveAway(killer, AVOID_RETREAT_STEP, false);
}
