/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PVPVALUES_H
#define PLAYERBOTS_PVPVALUES_H

#include "NamedObjectContext.h"
#include "SharedDefines.h"
#include "Value.h"

class PlayerbotAI;
class Unit;

class BgTypeValue : public ManualSetValue<uint32>
{
public:
    BgTypeValue(PlayerbotAI* botAI) : ManualSetValue<uint32>(botAI, 0, "bg type") {}
};

class ArenaTypeValue : public ManualSetValue<uint32>
{
public:
    ArenaTypeValue(PlayerbotAI* botAI) : ManualSetValue<uint32>(botAI, 0, "arena type") {}
};

class BgRoleValue : public ManualSetValue<uint32>
{
public:
    BgRoleValue(PlayerbotAI* botAI) : ManualSetValue<uint32>(botAI, 0, "bg role") {}
};

class BgMastersValue : public SingleCalculatedValue<std::vector<CreatureData const*>>, public Qualified
{
public:
    BgMastersValue(PlayerbotAI* botAI) : SingleCalculatedValue<std::vector<CreatureData const*>>(botAI, "bg masters") {}

    std::vector<CreatureData const*> Calculate() override;
};

class BgMasterValue : public CDPairCalculatedValue, public Qualified
{
public:
    BgMasterValue(PlayerbotAI* botAI) : CDPairCalculatedValue(botAI, "bg master", 60) {}

    CreatureData const* Calculate() override;
    CreatureData const* NearestBm(bool allowDead = true);
};

class RpgBgTypeValue : public CalculatedValue<BattlegroundTypeId>
{
public:
    RpgBgTypeValue(PlayerbotAI* botAI) : CalculatedValue(botAI, "rpg bg type") {}

    BattlegroundTypeId Calculate() override;
};

class FlagCarrierValue : public UnitCalculatedValue
{
public:
    FlagCarrierValue(PlayerbotAI* botAI, bool sameTeam = false, bool ignoreRange = false)
        : UnitCalculatedValue(botAI), sameTeam(sameTeam), ignoreRange(ignoreRange)
    {
    }

    Unit* Calculate() override;

private:
    bool sameTeam;
    bool ignoreRange;
};

class TeamFlagCarrierAttackerValue : public UnitCalculatedValue
{
public:
    TeamFlagCarrierAttackerValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "team fc attacker") {}

    Unit* Calculate() override;
};

class NearEnemyFlagRoomValue : public BoolCalculatedValue
{
public:
    NearEnemyFlagRoomValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "near enemy flag room") {}

    bool Calculate() override;
};

class EnemiesNearOwnFlagRoomValue : public Uint32CalculatedValue
{
public:
    EnemiesNearOwnFlagRoomValue(PlayerbotAI* botAI) : Uint32CalculatedValue(botAI, "enemies near own flag room") {}

    uint32 Calculate() override;
};

#endif
