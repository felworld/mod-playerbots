#include "WpvpTriggers.h"

#include "BotDeathSafety.h"
#include "NewRpgInfo.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Timer.h"
#include "WpvpAssist.h"
#include "WpvpTruce.h"

Unit* WpvpGoadTrigger::FindMark(PlayerbotAI* botAI, Player* bot)
{
    // Stealthers sneak up close before revealing; everyone else taunts from
    // wherever the emote can be seen.
    bool stealther = bot->getClass() == CLASS_ROGUE || bot->getClass() == CLASS_DRUID;
    float range = stealther ? STEALTH_GOAD_RANGE : OPEN_GOAD_RANGE;

    GuidVector targets =
        botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest unflagged enemy players")->Get();
    for (ObjectGuid const guid : targets)
    {
        Unit* enemy = botAI->GetUnit(guid);
        if (!enemy || !enemy->IsAlive() || bot->GetDistance(enemy) >= range)
            continue;

        // Same-class truce (Felworld): a fellow initiate isn't goad material
        // either - salute them instead.
        Player* enemyPlayer = enemy->ToPlayer();
        if (enemyPlayer && WpvpTruceHolds(bot, enemyPlayer))
        {
            WpvpTruceBoard::instance().NotePassing(bot, enemyPlayer);
            continue;
        }

        return enemy;
    }

    return nullptr;
}

bool WpvpGoadTrigger::IsActive()
{
    auto* data = std::get_if<NewRpgInfo::GoWpvp>(&botAI->rpgInfo.data);
    if (!data || !data->arrivedT)
        return false;

    // Defenders came for the reported ganker, not to taunt bystanders.
    if (data->defend)
        return false;

    if (bot->IsInCombat())
        return false;

    if (bot->getClass() == CLASS_ROGUE || bot->getClass() == CLASS_DRUID)
    {
        // Goading from stealth - the reveal IS the provocation.
        if (!botAI->HasAura("stealth", bot) && !botAI->HasAura("prowl", bot))
            return false;
    }
    else if (bot->HasStealthAura())
    {
        // Shadowmelded: staying hidden for the ambush beats taunting.
        return false;
    }

    if (data->lastGoadT &&
        GetMSTimeDiffToNow(data->lastGoadT) < sPlayerbotAIConfig.wpvpGoadCooldown * IN_MILLISECONDS)
        return false;

    // A flagged target already in reach takes priority over goading.
    if (AI_VALUE(Unit*, "enemy player target"))
        return false;

    return FindMark(botAI, bot) != nullptr;
}

bool WpvpShadowmeldTrigger::IsActive()
{
    // Classes with a real stealth of their own don't need the racial
    if (bot->getClass() == CLASS_ROGUE || bot->getClass() == CLASS_DRUID)
        return false;

    constexpr uint32 SPELL_SHADOWMELD = 58984;
    if (!bot->HasSpell(SPELL_SHADOWMELD) || bot->HasSpellCooldown(SPELL_SHADOWMELD))
        return false;

    auto* data = std::get_if<NewRpgInfo::GoWpvp>(&botAI->rpgInfo.data);
    if (!data || !data->arrivedT)
        return false;

    // Shadowmeld only holds while standing still, and is pointless when
    // already hidden
    if (bot->IsInCombat() || bot->isMoving() || bot->HasStealthAura())
        return false;

    return true;
}

bool WpvpRaidTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.wpvpRaidChance)
        return false;

    auto* data = std::get_if<NewRpgInfo::GoWpvp>(&botAI->rpgInfo.data);
    if (!data || !data->arrivedT || data->raidRolled)
        return false;

    // Defenders came to stop trouble, not to start it.
    if (data->defend)
        return false;

    if (bot->IsInCombat())
        return false;

    // Bored: dwelt out the whole boredom window...
    if (GetMSTimeDiffToNow(data->arrivedT) < sPlayerbotAIConfig.wpvpRaidBoredomSeconds * IN_MILLISECONDS)
        return false;

    // ...and still nobody to fight anywhere on screen.
    return !BotDeathSafety::EnemyPlayerNear(bot, sPlayerbotAIConfig.wpvpVisionDistance);
}

bool WpvpPeelTrigger::IsActive()
{
    if (sPlayerbotAIConfig.wpvpPeelAdvantageYards <= 0.0f || bot->InBattleground() || bot->InArena())
        return false;

    Unit* current = AI_VALUE(Unit*, "current target");
    if (!current || !current->IsPlayer() || !current->IsAlive())
        return false;

    Unit* alternative = AI_VALUE(Unit*, "enemy player target");
    if (!alternative || alternative == current || !alternative->IsPlayer())
        return false;

    // Same score as EnemyPlayerValue's sighting sort - distance with
    // kill-the-add pull - so the trigger can never disagree with what the
    // value would pick.
    return WpvpTargetScore(bot, alternative->ToPlayer()) + sPlayerbotAIConfig.wpvpPeelAdvantageYards <=
           WpvpTargetScore(bot, current->ToPlayer());
}
