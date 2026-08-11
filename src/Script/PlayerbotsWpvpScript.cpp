/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "AllCreatureScript.h"
#include "Creature.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ScriptMgr.h"
#include "WpvpChase.h"
#include "WpvpDefense.h"
#include "WpvpEmoteAlert.h"
#include "WpvpGrudge.h"
#include "WpvpSatiation.h"

// Feeds the wpvp boards from player hooks: world PvP kills roll the killer
// bot's satiation dice (anti-corpse-camping) and bump the killer's
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
        if (!killer || !killed || killer == killed)
            return;

        if (killer->InBattleground() || killer->InArena())
            return;

        if (killer->GetTeamId() == killed->GetTeamId())
            return;

        // The killing bot may roll "satiated" and stop initiating against
        // this victim for a while - the anti-corpse-camping grace.
        WpvpSatiationBoard::instance().RecordKill(killer, killed);

        // Victim side: a killed bot remembers its killer - out for revenge
        // or keeping well away - and a kill settles any grudge the killer
        // held against this victim.
        WpvpGrudgeBoard::instance().RecordKill(killer, killed);

        if (sPlayerbotAIConfig.wpvpCalloutEnabled || sPlayerbotAIConfig.wpvpDefenseEnabled ||
            sPlayerbotAIConfig.wpvpReinforcementEnabled)
        {
            WpvpDefenseBoard::instance().RecordKill(killer, killed);
            WpvpDefenseBoard::instance().RecordAttackerDeath(killed, killer->GetGUID());
        }
    }

    // A targeted emote at an enemy player is visible intel: the packet the
    // clients get names the target only as a localized string, but this hook
    // fires before the broadcast with the real guid. Witness bots pick the
    // sighting up from the emote-alert board.
    void OnPlayerTextEmote(Player* player, uint32 textEmote, uint32 /*emoteNum*/, ObjectGuid guid) override
    {
        NoteTargetedEmoteAtEnemy(player, textEmote, guid);

        // A beg/cry/shoo from someone under attack may move bot attackers
        // to mercy.
        NoteMercyPlea(player, textEmote, guid);
    }
};

// Damage between opposing players is what "contact" means to the chase leash:
// it keeps a pursuit's contact clock fresh (no break rolls while blows still
// land) and re-arms an abandoned chase when the runner swings again.
class PlayerbotsWpvpUnitScript : public UnitScript
{
public:
    PlayerbotsWpvpUnitScript() : UnitScript("PlayerbotsWpvpUnitScript", true, { UNITHOOK_ON_DAMAGE }) {}

    void OnDamage(Unit* attacker, Unit* victim, uint32& /*damage*/) override
    {
        WpvpChaseBoard::instance().NoteDamage(attacker, victim);
    }
};

// The core calls OnZoneUnderAttack alongside the faction-wide "X is under
// attack!" broadcast (a guard dying to a player, scripted town alarms). Real
// players get the yellow text; bots get the same news as a defense-board
// entry - no bot chat, the server already made the announcement.
class PlayerbotsWpvpCreatureScript : public AllCreatureScript
{
public:
    PlayerbotsWpvpCreatureScript() : AllCreatureScript("PlayerbotsWpvpCreatureScript") {}

    void OnZoneUnderAttack(Creature* creature, Player* attacker) override
    {
        if (!sPlayerbotAIConfig.wpvpNpcAttackDefense)
            return;

        if (!sPlayerbotAIConfig.wpvpCalloutEnabled && !sPlayerbotAIConfig.wpvpDefenseEnabled &&
            !sPlayerbotAIConfig.wpvpReinforcementEnabled)
            return;

        // Responders can only travel to the open world; a scripted alarm
        // inside an instance is not somewhere the cavalry can ride.
        if (!attacker->IsInWorld() || attacker->GetMap()->Instanceable())
            return;

        // Mirror the core's broadcast audience: the attacker's opposing team.
        TeamId defendingTeam = attacker->GetTeamId() == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE;
        WpvpDefenseBoard::instance().RecordZoneUnderAttack(attacker, defendingTeam, creature->GetLevel());
    }
};

void AddPlayerbotsWpvpScripts()
{
    new PlayerbotsWpvpScript();
    new PlayerbotsWpvpUnitScript();
    new PlayerbotsWpvpCreatureScript();
}
