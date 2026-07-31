/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CLASSSERVICEACTIONS_H
#define PLAYERBOTS_CLASSSERVICEACTIONS_H

#include "InventoryAction.h"

class PlayerbotAI;

// Class "service" commands: a mage conjures food/water and hands it to the requester,
// a mage opens a city portal, a warlock performs a summoning ritual on the requester
// (recruiting nearby group bots to click the portal).

class ConjureItemAction : public InventoryAction
{
public:
    ConjureItemAction(PlayerbotAI* botAI) : InventoryAction(botAI, "conjure") {}

    bool Execute(Event event) override;

private:
    uint32 FindConjureSpell(bool drink);
    std::vector<Item*> CollectConjured(bool drink, bool includeOtherCategory);
    bool GiveConjured(Player* requester, std::vector<Item*> const& items);
    bool Requeue(Player* requester, std::string const& kind, uint32 retriesLeft);
};

class OpenPortalAction : public Action
{
public:
    OpenPortalAction(PlayerbotAI* botAI) : Action(botAI, "portal") {}

    bool Execute(Event event) override;
};

class RitualOfSummoningAction : public Action
{
public:
    RitualOfSummoningAction(PlayerbotAI* botAI) : Action(botAI, "ritual") {}

    bool Execute(Event event) override;

private:
    bool InviteRequester(Player* requester);
    bool WaitForAccept(Player* requester, uint32 retriesLeft);
    bool PerformRitual(Player* requester);
};

class UseSummoningPortalAction : public Action
{
public:
    UseSummoningPortalAction(PlayerbotAI* botAI) : Action(botAI, "use summoning portal") {}

    bool Execute(Event event) override;
};

// Internal: a bot recruited into a group for a summoning ritual leaves it again.
class RitualDepartAction : public Action
{
public:
    RitualDepartAction(PlayerbotAI* botAI) : Action(botAI, "ritual depart") {}

    bool Execute(Event event) override;
};

#endif
