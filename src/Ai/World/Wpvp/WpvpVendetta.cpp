#include "WpvpVendetta.h"

#include "DatabaseEnv.h"
#include "FelworldEvents.h"
#include "GameTime.h"
#include "LevelPerception.h"
#include "Metric.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "StringFormat.h"
#include "Timer.h"

namespace
{
// A re-kill this soon after the last one is camping: the victim barely had
// time to wait the camper out (BotDeathSafety's patience) and run back
// before dying again.
constexpr uint32 CAMP_WINDOW_SECONDS = 10 * MINUTE;

// How long after picking a fight the bot still owns its outcome; kept
// fresh by RefreshInitiated while the bot keeps landing blows.
constexpr uint32 INITIATED_TTL_MS = 2 * MINUTE * IN_MILLISECONDS;

// One plea per open vendetta per this long - the grudge board's beat.
constexpr uint32 PLEA_COOLDOWN_MS = 20 * IN_MILLISECONDS;

uint32 EpochNow()
{
    return uint32(GameTime::GetGameTime().count());
}
}

void WpvpVendettaBoard::LoadFromDB()
{
    if (!sPlayerbotAIConfig.wpvpVendettaGanks)
        return;

    uint32 count = 0;

    std::lock_guard<std::mutex> lock(_mutex);

    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_WPVP_VENDETTA);
    if (PreparedQueryResult result = PlayerbotsDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();
            ObjectGuid const victim = ObjectGuid::Create<HighGuid::Player>(fields[0].Get<uint32>());
            ObjectGuid const killer = ObjectGuid::Create<HighGuid::Player>(fields[1].Get<uint32>());

            Vendetta& vendetta = _ledger[victim][killer];
            vendetta.ganks = fields[2].Get<uint32>();
            vendetta.camps = fields[3].Get<uint32>();
            vendetta.lastGankAt = fields[4].Get<uint32>();
            vendetta.settled = fields[5].Get<uint8>() != 0;
            ++count;
        } while (result->NextRow());
    }

    LOG_INFO("playerbots", "Loaded {} wpvp vendetta ledger entries", count);
}

void WpvpVendettaBoard::NoteBotInitiated(Player* bot, Player* enemy)
{
    if (!sPlayerbotAIConfig.wpvpVendettaGanks || !bot || !enemy)
        return;

    uint32 const now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);
    _initiated[bot->GetGUID()][enemy->GetGUID()] = now + INITIATED_TTL_MS;
}

void WpvpVendettaBoard::RefreshInitiated(Unit* attacker, Unit* victim)
{
    if (!sPlayerbotAIConfig.wpvpVendettaGanks || !attacker || !victim)
        return;

    if (!attacker->IsPlayer() || !victim->IsPlayer())
        return;

    uint32 const now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);

    auto held = _initiated.find(attacker->GetGUID());
    if (held == _initiated.end())
        return;

    auto it = held->second.find(victim->GetGUID());
    if (it == held->second.end() || int32(it->second - now) <= 0)
        return;

    it->second = now + INITIATED_TTL_MS;
}

void WpvpVendettaBoard::RecordKill(Player* killer, Player* victim)
{
    if (!sPlayerbotAIConfig.wpvpVendettaGanks)
        return;

    uint32 const nowMs = getMSTime();
    uint32 const now = EpochNow();

    std::lock_guard<std::mutex> lock(_mutex);

    // Vengeance delivered: the killer's open vendetta against this victim
    // is settled. The tally stays on the books - one fresh gank re-opens it.
    if (auto held = _ledger.find(killer->GetGUID()); held != _ledger.end())
    {
        auto it = held->second.find(victim->GetGUID());
        if (it != held->second.end() && Open(it->second))
        {
            it->second.settled = true;
            Persist(killer->GetGUID(), victim->GetGUID(), it->second);

            METRIC_VALUE("playerbots_wpvp_vendetta", 1, METRIC_TAG("event", "settled"));
            LOG_DEBUG("playerbots", "Bot {} settles its vendetta against {}", killer->GetName(), victim->GetName());
            Felworld::LogEvent(killer->GetGUID(), "wpvp_vendetta_settled",
                               Acore::StringFormat("{{\"victim\":\"{}\"}}", victim->GetName()));
        }
    }

    // Only bots keep a ledger - a real player's resentment is their own.
    if (!GET_PLAYERBOT_AI(victim))
        return;

    // Death voids whatever fights the victim had picked - and a death
    // inside one of them was the bot's own doing: it tallies nothing.
    bool chosen = false;
    if (auto marks = _initiated.find(victim->GetGUID()); marks != _initiated.end())
    {
        auto it = marks->second.find(killer->GetGUID());
        chosen = it != marks->second.end() && int32(it->second - nowMs) > 0;
        _initiated.erase(marks);
    }

    if (chosen)
        return;

    Vendetta& vendetta = _ledger[victim->GetGUID()][killer->GetGUID()];
    bool const wasOpen = Open(vendetta);
    bool const camped = vendetta.lastGankAt && now - vendetta.lastGankAt <= CAMP_WINDOW_SECONDS;
    vendetta.ganks++;
    if (camped)
        vendetta.camps++;
    vendetta.lastGankAt = now;
    vendetta.settled = false;
    Persist(victim->GetGUID(), killer->GetGUID(), vendetta);

    if (!wasOpen && Open(vendetta))
    {
        METRIC_VALUE("playerbots_wpvp_vendetta", 1, METRIC_TAG("event", "opened"));
        LOG_DEBUG("playerbots", "Bot {} opens a vendetta against {} ({} ganks, {} camped)", victim->GetName(),
                  killer->GetName(), vendetta.ganks, vendetta.camps);
        Felworld::LogEvent(victim->GetGUID(), "wpvp_vendetta",
                           Acore::StringFormat("{{\"killer\":\"{}\",\"ganks\":{},\"camps\":{}}}", killer->GetName(),
                                               vendetta.ganks, vendetta.camps));
    }
}

WpvpGrudgeDisposition WpvpVendettaBoard::Disposition(Player* bot, Player* enemy)
{
    if (!sPlayerbotAIConfig.wpvpVendettaGanks || !bot || !enemy)
        return WpvpGrudgeDisposition::None;

    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto held = _ledger.find(bot->GetGUID());
        if (held == _ledger.end())
            return WpvpGrudgeDisposition::None;

        auto it = held->second.find(enemy->GetGUID());
        if (it == held->second.end() || !Open(it->second))
            return WpvpGrudgeDisposition::None;
    }

    // Outside the lock - PerceivedLevel reads world state. A tormentor who
    // still plainly outclasses the bot is feared, not hunted; the fear
    // turns to vengeance once the bot catches up.
    if (int32(PerceivedLevel(bot, enemy)) - int32(bot->GetLevel()) >= WPVP_REVENGE_OUTCLASS_GAP)
        return WpvpGrudgeDisposition::Avoidant;

    return WpvpGrudgeDisposition::Revenge;
}

std::vector<ObjectGuid> WpvpVendettaBoard::VendettaEnemies(ObjectGuid bot)
{
    std::vector<ObjectGuid> enemies;

    if (!sPlayerbotAIConfig.wpvpVendettaGanks)
        return enemies;

    std::lock_guard<std::mutex> lock(_mutex);

    auto held = _ledger.find(bot);
    if (held == _ledger.end())
        return enemies;

    for (auto const& [enemy, vendetta] : held->second)
        if (Open(vendetta))
            enemies.push_back(enemy);

    return enemies;
}

bool WpvpVendettaBoard::ClaimPleaEmote(ObjectGuid bot, ObjectGuid enemy)
{
    uint32 const now = getMSTime();

    std::lock_guard<std::mutex> lock(_mutex);

    auto held = _ledger.find(bot);
    if (held == _ledger.end())
        return false;

    auto it = held->second.find(enemy);
    if (it == held->second.end() || !Open(it->second))
        return false;

    if (it->second.nextPleaMs && int32(it->second.nextPleaMs - now) > 0)
        return false;

    it->second.nextPleaMs = now + PLEA_COOLDOWN_MS;
    return true;
}

bool WpvpVendettaBoard::Open(Vendetta const& vendetta) const
{
    uint32 const ganks = sPlayerbotAIConfig.wpvpVendettaGanks;
    uint32 const campGanks = sPlayerbotAIConfig.wpvpVendettaCampGanks;

    if (!ganks || vendetta.settled)
        return false;

    return vendetta.ganks >= ganks || (campGanks && vendetta.camps && vendetta.ganks >= campGanks);
}

void WpvpVendettaBoard::Persist(ObjectGuid victim, ObjectGuid killer, Vendetta const& vendetta)
{
    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_REP_WPVP_VENDETTA);
    stmt->SetData(0, victim.GetCounter());
    stmt->SetData(1, killer.GetCounter());
    stmt->SetData(2, vendetta.ganks);
    stmt->SetData(3, vendetta.camps);
    stmt->SetData(4, vendetta.lastGankAt);
    stmt->SetData(5, uint8(vendetta.settled ? 1 : 0));
    PlayerbotsDatabase.Execute(stmt);
}
