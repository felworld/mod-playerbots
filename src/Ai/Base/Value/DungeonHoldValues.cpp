/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DungeonHoldValues.h"

#include "Group.h"
#include "ObjectAccessor.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Timer.h"

namespace
{
    // A mob the bot has not looked at for a minute is dead, gone or somebody else's problem; sweeping
    // every ten seconds keeps the map bounded without walking it on every evaluation.
    constexpr uint32 TANK_ENGAGE_ENTRY_LIFETIME = 60 * IN_MILLISECONDS;
    constexpr uint32 TANK_ENGAGE_PRUNE_INTERVAL = 10 * IN_MILLISECONDS;

    // The main tank this bot is waiting for, or nullptr when there is nothing to wait for - the hold
    // is off, this is not instanced group content, the bot is the main tank itself, or no main tank
    // can be resolved at all. Never storing the result past the call: it is a live pointer.
    Player* GetHoldMainTank(PlayerbotAI* botAI)
    {
        if (!botAI || !sPlayerbotAIConfig.dungeonHoldForTank)
            return nullptr;

        Player* bot = botAI->GetBot();
        if (!IsInstancedGroupContent(bot))
            return nullptr;

        ObjectGuid const mainTankGuid = PlayerbotAI::GetMainTankGuid(bot->GetGroup());
        if (mainTankGuid.IsEmpty() || mainTankGuid == bot->GetGUID())
            return nullptr;

        Player* mainTank = ObjectAccessor::FindPlayer(mainTankGuid);
        if (!mainTank || !mainTank->IsInWorld() || !mainTank->IsAlive() || mainTank->GetMap() != bot->GetMap())
            return nullptr;

        return mainTank;
    }

    // The mob is already beating on somebody in the group other than the main tank. A pet counts as
    // its owner: whoever it belongs to is the one who now has a problem.
    bool HasPeeledOffTheTank(Group* group, Unit* victim, Player* mainTank)
    {
        if (!group || !victim)
            return false;

        Unit* owner = victim->GetCharmerOrOwner();
        Player* member = owner ? owner->ToPlayer() : victim->ToPlayer();
        return member && member != mainTank && group->IsMember(member->GetGUID());
    }
}

void TankEngageMemory::Prune(uint32 now)
{
    if (lastPrune && getMSTimeDiff(lastPrune, now) < TANK_ENGAGE_PRUNE_INTERVAL)
        return;

    lastPrune = now;

    for (auto itr = entries.begin(); itr != entries.end();)
    {
        if (getMSTimeDiff(itr->second.lastSeen, now) >= TANK_ENGAGE_ENTRY_LIFETIME)
            itr = entries.erase(itr);
        else
            ++itr;
    }
}

bool IsDungeonHoldActive(PlayerbotAI* botAI) { return GetHoldMainTank(botAI) != nullptr; }

bool ShouldHoldForTank(PlayerbotAI* botAI, Unit* target)
{
    Player* mainTank = GetHoldMainTank(botAI);
    if (!mainTank)
        return false;

    // Players have no threat table for the tank to build on, so there is nothing to wait for.
    if (!target || !target->IsAlive() || target->IsPlayer())
        return false;

    Player* bot = botAI->GetBot();
    if (!bot->IsValidAttackTarget(target))
        return false;

    uint32 const now = getMSTime();

    TankEngageMemory& memory = botAI->GetAiObjectContext()->GetValue<TankEngageMemory&>("tank engage memory")->Get();
    memory.Prune(now);

    TankEngageInfo& info = memory.entries[target->GetGUID()];
    info.lastSeen = now;

    // Nobody is fighting it yet, so opening on it would be the pull the main tank is there to make.
    if (!target->IsInCombat())
    {
        info.firstInTankMelee = 0;
        info.combatStart = 0;
        return true;
    }

    if (!info.combatStart)
        info.combatStart = now;

    Unit* victim = target->GetVictim();

    // (2) It peeled onto a healer, a ranged bot or the bot itself. Holding now would only hand the
    // mob free time on somebody who cannot take it.
    if (HasPeeledOffTheTank(bot->GetGroup(), victim, mainTank))
        return false;

    // (1) The normal case: it ran to the tank and the tank has had the grace period to build threat.
    if (mainTank->IsWithinMeleeRange(target))
    {
        if (!info.firstInTankMelee)
            info.firstInTankMelee = now;

        if (victim == mainTank &&
            getMSTimeDiff(info.firstInTankMelee, now) >= sPlayerbotAIConfig.dungeonHoldEngageDelay)
            return false;
    }
    else
        info.firstInTankMelee = 0;

    // (3) Casters and ranged mobs shoot the tank from where they stand and never enter its melee
    // range, so melee time alone would hold the group off them forever.
    if (getMSTimeDiff(info.combatStart, now) >= sPlayerbotAIConfig.dungeonHoldTimeout)
        return false;

    return true;
}
