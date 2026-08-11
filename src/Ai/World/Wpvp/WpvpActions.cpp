#include "WpvpActions.h"

#include "CellImpl.h"
#include "FelworldEvents.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "LevelPerception.h"
#include "Metric.h"
#include "NewRpgInfo.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Random.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include "Timer.h"
#include "WpvpTriggers.h"

bool WpvpGoadAction::Execute(Event /*event*/)
{
    auto* data = std::get_if<NewRpgInfo::GoWpvp>(&botAI->rpgInfo.data);
    if (!data)
        return false;

    Unit* mark = WpvpGoadTrigger::FindMark(botAI, bot);
    if (!mark)
        return false;

    botAI->RemoveAura("stealth");
    botAI->RemoveAura("prowl");
    bot->SetFacingToObject(mark);

    static constexpr Emote provocations[] = {EMOTE_ONESHOT_RUDE, EMOTE_ONESHOT_CHICKEN, EMOTE_ONESHOT_POINT};
    bot->HandleEmoteCommand(provocations[urand(0, 2)]);

    data->lastGoadT = getMSTime();
    return true;
}

namespace
{
// A raid mark near the bot: hostile, alive, clearly outleveled, and either a
// guard or - knob permitting - a flight master (the classic bait).
class RaidMarkCheck
{
public:
    RaidMarkCheck(Player* bot) : _bot(bot) {}

    bool operator()(Creature* creature) const
    {
        if (!creature->IsAlive() || creature->IsInEvadeMode() || !creature->IsHostileTo(_bot))
            return false;

        if (!creature->IsGuard() && !(sPlayerbotAIConfig.wpvpRaidFlightMasters && creature->IsTaxi()))
            return false;

        // Raids are bait, not suicide: only a target the bot clearly
        // outlevels is worth poking the whole town over.
        return PerceivedLevel(_bot, creature) + sPlayerbotAIConfig.wpvpGankLevelGap <= _bot->GetLevel();
    }

private:
    Player* _bot;
};

Creature* FindRaidMark(Player* bot)
{
    // The bot dwells 40-80yd off the hub, so vision range from where it
    // stands covers the hub's guard posts.
    std::list<Creature*> marks;
    RaidMarkCheck check(bot);
    Acore::CreatureListSearcher<RaidMarkCheck> searcher(bot, marks, check);
    Cell::VisitObjects(bot, searcher, sPlayerbotAIConfig.wpvpVisionDistance);

    Creature* nearest = nullptr;
    for (Creature* mark : marks)
        if (!nearest || bot->GetDistance(mark) < bot->GetDistance(nearest))
            nearest = mark;

    return nearest;
}
}

bool WpvpRaidAction::Execute(Event /*event*/)
{
    auto* data = std::get_if<NewRpgInfo::GoWpvp>(&botAI->rpgInfo.data);
    if (!data)
        return false;

    // One roll per excursion, pass or fail - most bored invaders just keep
    // lurking.
    data->raidRolled = true;
    if (!roll_chance_i(sPlayerbotAIConfig.wpvpRaidChance))
        return true;

    Creature* mark = FindRaidMark(bot);
    if (!mark)
        return true;

    METRIC_VALUE("playerbots_wpvp", 1, METRIC_TAG("event", "raid_npc"));
    Felworld::LogEvent(bot->GetGUID(), "wpvp_raid_npc",
                       Acore::StringFormat("{{\"zone\":{},\"npc\":\"{}\"}}", data->zoneId, mark->GetName()));

    return Attack(mark);
}
