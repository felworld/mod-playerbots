/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_FORMATIONS_H
#define PLAYERBOTS_FORMATIONS_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "PlayerbotAIConfig.h"
#include "TravelMgr.h"

class Map;
class Player;
class PlayerbotAI;

class Formation : public AiNamedObject
{
public:
    Formation(PlayerbotAI* botAI, std::string const name) : AiNamedObject(botAI, name) {}
    virtual ~Formation() = default;
    virtual std::string const GetTargetName() { return ""; }
    virtual WorldLocation GetLocation() { return NullLocation; }
    virtual float GetMaxDistance() { return sPlayerbotAIConfig.followDistance; }
    static WorldLocation NullLocation;
    static bool IsNullLocation(WorldLocation const& loc);

protected:
    float GetFollowAngle();
    // Angle of the tight follow slot: the rear arc behind the master in instanced group content, the
    // plain formation ring everywhere else.
    float GetFollowSlotAngle();

    // Dungeon follow spread (Felworld): in instanced group content ranged bots and healers hold their
    // own attack/heal range behind the master instead of stacking on him, so walking up to a pack does
    // not drag the whole group - and their pets - into the packs next door.
    // Returns false (and leaves location untouched) whenever the bot should keep the tight follow slot.
    bool GetDungeonSpreadLocation(Player* master, WorldLocation& location);
    // Slack for "close enough to my slot" while spread out; 0 when the bot is not spreading.
    float GetDungeonSpreadMaxDistance() const;

private:
    float GetDungeonSpreadRange();
    bool IsDungeonSpreadSpotSafe(Player* master, Map* map, float angle, float range);

    // Rear-arc following (Felworld): the bot's slot folded into a fan behind the master, keyed on a
    // smoothed "behind" direction rather than his live facing so the group does not orbit him every
    // time he glances at a wall.
    float GetDungeonRearAngle(Player* master);
    float GetDungeonRearFacing(Player* master);

    float spreadRange = 0.0f;  // resolved radius from the master, 0 while not spreading
    float spreadAngle = 0.0f;  // absolute (world) angle of the spread slot
    time_t spreadTime = 0;
    uint32 spreadMasterMapId = 0;
    float spreadMasterX = 0.0f;
    float spreadMasterY = 0.0f;

    bool rearFacingKnown = false;   // false until the first sample of the master's heading
    float rearFacing = 0.0f;        // smoothed direction the master is heading/looking
    uint32 rearFacingMapId = 0;
    float rearFacingX = 0.0f;       // master's position when rearFacing was last resampled
    float rearFacingY = 0.0f;
};

class FollowFormation : public Formation
{
public:
    FollowFormation(PlayerbotAI* botAI, std::string const name) : Formation(botAI, name) {}
};

class MoveFormation : public Formation
{
public:
    MoveFormation(PlayerbotAI* botAI, std::string const name) : Formation(botAI, name) {}

protected:
    WorldLocation MoveLine(std::vector<Player*> line, float diff, float cx, float cy, float cz, float orientation,
                           float range);
    WorldLocation MoveSingleLine(std::vector<Player*> line, float diff, float cx, float cy, float cz, float orientation,
                                 float range);
};

class MoveAheadFormation : public MoveFormation
{
public:
    MoveAheadFormation(PlayerbotAI* botAI, std::string const name) : MoveFormation(botAI, name) {}

    WorldLocation GetLocation() override;
    virtual WorldLocation GetLocationInternal() { return NullLocation; }
};

class FormationValue : public ManualSetValue<Formation*>
{
public:
    FormationValue(PlayerbotAI* botAI);
    ~FormationValue();

    std::string const Save() override;
    bool Load(std::string const value) override;
};

class SetFormationAction : public Action
{
public:
    SetFormationAction(PlayerbotAI* botAI) : Action(botAI, "set formation") {}

    bool Execute(Event event) override;
};

#endif
