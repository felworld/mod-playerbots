#include "WpvpReadiness.h"

#include <unordered_map>

#include "FelworldEvents.h"
#include "LevelPerception.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include "Timer.h"
#include "WpvpAssist.h"
#include "WpvpGrudge.h"

namespace
{
// A target this far below the bot's perceived read is a kill on any bars.
constexpr int32 EASY_MARK_LEVEL_GAP = 5;

// How long a gated sighting keeps the drink-up behavior armed.
constexpr uint32 GATED_OPPORTUNITY_WINDOW_MS = 30 * IN_MILLISECONDS;

std::unordered_map<ObjectGuid, uint32> gatedOpportunityMs;
}

bool WpvpBarsReadyToInitiate(Player* bot)
{
    uint32 const healthBar = sPlayerbotAIConfig.wpvpInitiateSelfHealth;
    if (healthBar && bot->GetHealthPct() < float(healthBar))
        return false;

    uint32 const manaBar = sPlayerbotAIConfig.wpvpInitiateSelfMana;
    if (manaBar && bot->getPowerType() == POWER_MANA && bot->GetPowerPct(POWER_MANA) < float(manaBar))
        return false;

    return true;
}

bool WpvpReadyToInitiate(Player* bot, Player* target)
{
    // Instanced PvP is exempt: everyone there already came to fight, and
    // target acquisition doubles as objective play.
    if (bot->InBattleground() || bot->InArena())
        return true;

    if (WpvpBarsReadyToInitiate(bot))
        return true;

    // "Now" can still beat "after I'm full up" when the advantage sits on
    // the target's side of the ledger instead of the bot's bars: they are
    // already tied up fighting other players...
    if (WpvpActivePvpCombatant(target))
        return true;

    // ...or so far below the bot that the bars don't matter...
    if (int32(bot->GetLevel()) - int32(PerceivedLevel(bot, target)) >= EASY_MARK_LEVEL_GAP)
        return true;

    // ...or they're the bot's killer, and revenge doesn't wait on a drink.
    return WpvpGrudgeAgainst(bot, target) == WpvpGrudgeDisposition::Revenge;
}

void WpvpNoteGatedOpportunity(Player* bot, Player* target)
{
    uint32 const now = getMSTime();
    uint32& last = gatedOpportunityMs[bot->GetGUID()];
    bool const freshWindow = !last || getMSTimeDiff(last, now) > GATED_OPPORTUNITY_WINDOW_MS;
    last = now;

    // The gate re-declines every AI tick while the target stays in sight;
    // only the first pass of a quiet window is worth a line.
    if (!freshWindow)
        return;

    uint32 const healthPct = uint32(bot->GetHealthPct());
    uint32 const manaPct =
        bot->getPowerType() == POWER_MANA ? uint32(bot->GetPowerPct(POWER_MANA)) : 100;
    LOG_DEBUG("playerbots", "Bot {} passes on initiating against {}: {}% health, {}% mana",
              bot->GetName(), target->GetName(), healthPct, manaPct);
    Felworld::LogEvent(bot->GetGUID(), "wpvp_initiate_gated",
                       Acore::StringFormat("{{\"target\":\"{}\",\"health\":{},\"mana\":{}}}",
                                           target->GetName(), healthPct, manaPct));
}

bool WpvpGatedOpportunityRecent(Player* bot)
{
    auto it = gatedOpportunityMs.find(bot->GetGUID());
    return it != gatedOpportunityMs.end() &&
           getMSTimeDiff(it->second, getMSTime()) <= GATED_OPPORTUNITY_WINDOW_MS;
}
