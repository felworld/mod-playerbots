#ifndef PLAYERBOTS_NEWRPGWPVP_H
#define PLAYERBOTS_NEWRPGWPVP_H

#include "NewRpgBaseAction.h"

// Sample a ground position distMin..distMax yards from the hub at
// bearing +/- bearingSpread, rejecting spots whose ground height differs
// from the hub's by more than zTolerance (roofs, cliffs, cellars).
bool SampleGroundNear(Map* map, WorldLocation const& hub, float bearing, float bearingSpread, float distMin,
                      float distMax, float zTolerance, WorldPosition& out);

// Unlike PlayerbotAI::HasPlayerNearby, which only considers real players on
// the bot's CURRENT map, this checks against the position's own map - needed
// for pre-teleport guards, where the arrival point is usually far from the
// bot.
bool RealPlayerNear(WorldPosition& pos, float range);

// Compute the dwell anchor (WpvpAnchorOffset yards off the hub) and the
// teleport arrival point (WpvpTeleportOffset yards out on the same bearing)
// for a world-PvP excursion. hubPos/zoneId/anchorPos/teleportPos are filled
// in; the movement target is always the anchor, never the hub itself, so a
// stuck-teleport recovery blinks the bot to a field instead of into town.
// Returns false if the hub's map isn't loaded.
bool ComputeWpvpPositions(WorldLocation const& hubLoc, uint32 zoneId, NewRpgInfo::GoWpvp& out);

// The bot's current wpvp excursion is a defend-mode one - a defense
// responder or a reinforcer, not an invader. False when the bot isn't on
// an excursion at all.
bool WpvpOnDefenseMission(PlayerbotAI* botAI);

// Shared teardown for a world-PvP excursion (also used by the GM kill
// switch): removes the excursion strategies and returns the bot to idle.
// The PvP flag is left to decay naturally. The reason is logged (INFO for
// test excursions, DEBUG otherwise).
void EndWpvpExcursion(PlayerbotAI* botAI, char const* reason);

// How a destination zone reads to an invading bot under the excursion rules.
enum class WpvpZoneCategory : uint8
{
    None,           // ineligible (wrong level, own-faction zone, prohibited, same zone)
    Contested,      // contested zone within the bot's level bracket
    LowerBracket,   // contested zone the bot overlevels by WpvpGankerMinLevelGap+
    EnemyHomeZone,  // enemy home zone the bot overlevels by WpvpHomeZoneMinLevelGap+
};

// Classify a destination zone for an invading bot - the single source of the
// eligibility rules, shared by organic destination selection and the
// "wpvp test" GM command. homeWeight gets the 0.25..1.0 level-gap curve
// value and is only meaningful for EnemyHomeZone.
WpvpZoneCategory ClassifyWpvpDestination(Player* invader, uint32 zoneId, uint32 areaTeam, uint32 bracketLow,
                                         uint32 bracketHigh, float& homeWeight);

class NewRpgGoWpvpAction : public NewRpgBaseAction
{
public:
    NewRpgGoWpvpAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg go wpvp") {}
    bool Execute(Event event) override;

private:
    bool GuardedTeleport(NewRpgInfo::GoWpvp& data);
    bool Dwell(NewRpgInfo::GoWpvp& data);
};

#endif
