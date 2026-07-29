/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ScriptMgr.h"
#include "WpvpDefense.h"
#include "WpvpEmoteAlert.h"

// Feeds the wpvp defense board: world PvP kills bump the killer's
// uncontested-kill tally (arming the WorldDefense escalation shout), and a
// tracked ganker's own death marks the spree contested - and, if the killer
// was outside help rather than a victim, counts toward the ganker's own
// faction sending a reinforcement wave.
class PlayerbotsWpvpScript : public PlayerScript
{
public:
    PlayerbotsWpvpScript()
        : PlayerScript("PlayerbotsWpvpScript", { PLAYERHOOK_ON_PVP_KILL, PLAYERHOOK_ON_TEXT_EMOTE })
    {
    }

    void OnPlayerPVPKill(Player* killer, Player* killed) override
    {
        if (!sPlayerbotAIConfig.wpvpCalloutEnabled && !sPlayerbotAIConfig.wpvpDefenseEnabled &&
            !sPlayerbotAIConfig.wpvpReinforcementEnabled)
            return;

        if (!killer || !killed || killer == killed)
            return;

        if (killer->InBattleground() || killer->InArena())
            return;

        if (killer->GetTeamId() == killed->GetTeamId())
            return;

        WpvpDefenseBoard::instance().RecordKill(killer, killed);
        WpvpDefenseBoard::instance().RecordAttackerDeath(killed, killer->GetGUID());
    }

    // A targeted emote at an enemy player is visible intel: the packet the
    // clients get names the target only as a localized string, but this hook
    // fires before the broadcast with the real guid. Witness bots pick the
    // sighting up from the emote-alert board.
    void OnPlayerTextEmote(Player* player, uint32 /*textEmote*/, uint32 /*emoteNum*/, ObjectGuid guid) override
    {
        NoteTargetedEmoteAtEnemy(player, guid);
    }
};

void AddPlayerbotsWpvpScripts()
{
    new PlayerbotsWpvpScript();
}
