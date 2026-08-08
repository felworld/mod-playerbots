#include "WpvpGuardRespect.h"

#include "CellImpl.h"
#include "DBCEnums.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "LevelPerception.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"

namespace
{
// How near a prohibitive guard has to stand for "under guard cover" to hold.
// Guard aggro radius against a 5+ level lower enemy runs up to ~45 yards, so
// closing to melee/spell range of anyone this close to a guard means entering
// it; the same radius around the bot trips just before the guard line.
constexpr float GUARD_COVER_RADIUS = 40.0f;

class OutlevelingHostileGuardCheck
{
public:
    OutlevelingHostileGuardCheck(Player* bot, uint32 minLevel) : _bot(bot), _minLevel(minLevel) {}

    bool operator()(Creature* creature) const
    {
        return creature->IsAlive() && creature->IsGuard() && PerceivedLevel(_bot, creature) >= _minLevel &&
               !creature->IsInEvadeMode() && creature->IsHostileTo(_bot);
    }

private:
    Player* _bot;
    uint32 _minLevel;
};

bool ProhibitiveGuardNear(WorldObject* center, Player* bot, uint32 minLevel)
{
    std::list<Creature*> guards;
    OutlevelingHostileGuardCheck check(bot, minLevel);
    Acore::CreatureListSearcher<OutlevelingHostileGuardCheck> searcher(center, guards, check);
    Cell::VisitObjects(center, searcher, GUARD_COVER_RADIUS);
    return !guards.empty();
}
}  // namespace

bool WpvpGuardsBarPursuit(Player* bot, Unit* target)
{
    uint32 const gap = sPlayerbotAIConfig.wpvpGuardRespectLevelGap;
    if (!gap || !target || !target->IsPlayer())
        return false;

    if (bot->InBattleground() || bot->InArena())
        return false;

    if (bot->duel && bot->duel->Opponent == target)
        return false;

    // No guard outlevels a near-max-level bot by the gap, so skip the grid
    // searches for the common high-level-ganker case.
    uint32 const minLevel = bot->GetLevel() + gap;
    if (minLevel > DEFAULT_MAX_LEVEL)
        return false;

    return ProhibitiveGuardNear(target, bot, minLevel) || ProhibitiveGuardNear(bot, bot, minLevel);
}
