/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ScriptMgr.h"
#include "WpvpDefense.h"

// Feeds the wpvp defense board: world PvP kills bump the killer's
// uncontested-kill tally (arming the WorldDefense escalation shout), and a
// tracked ganker's own death marks the spree contested.
class PlayerbotsWpvpScript : public PlayerScript
{
public:
    PlayerbotsWpvpScript() : PlayerScript("PlayerbotsWpvpScript", { PLAYERHOOK_ON_PVP_KILL }) {}

    void OnPlayerPVPKill(Player* killer, Player* killed) override
    {
        if (!sPlayerbotAIConfig.wpvpCalloutEnabled && !sPlayerbotAIConfig.wpvpDefenseEnabled)
            return;

        if (!killer || !killed || killer == killed)
            return;

        if (killer->InBattleground() || killer->InArena())
            return;

        if (killer->GetTeamId() == killed->GetTeamId())
            return;

        WpvpDefenseBoard::instance().RecordKill(killer, killed);
        WpvpDefenseBoard::instance().RecordAttackerDeath(killed->GetGUID());
    }
};

void AddPlayerbotsWpvpScripts()
{
    new PlayerbotsWpvpScript();
}
