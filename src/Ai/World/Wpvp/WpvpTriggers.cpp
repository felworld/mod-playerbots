#include "WpvpTriggers.h"

#include "NewRpgInfo.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Timer.h"

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
        if (enemy && enemy->IsAlive() && bot->GetDistance(enemy) < range)
            return enemy;
    }

    return nullptr;
}

bool WpvpGoadTrigger::IsActive()
{
    auto* data = std::get_if<NewRpgInfo::GoWpvp>(&botAI->rpgInfo.data);
    if (!data || !data->arrivedT)
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
