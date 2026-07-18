/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "NonCombatActions.h"

#include "Battleground.h"
#include "Event.h"
#include "Playerbots.h"
#include "SpellAuraEffects.h"
#include "SpellDefines.h"

namespace
{
constexpr uint32 BG_WS_SPELL_WARSONG_FLAG = 23333;
constexpr uint32 BG_WS_SPELL_SILVERWING_FLAG = 23335;
constexpr uint32 BG_EY_NETHERSTORM_FLAG_SPELL = 34976;

bool IsDisallowedShapeshiftForm(Player* bot)
{
    if (bot->getClass() == CLASS_DRUID)
    {
        ShapeshiftForm form = bot->GetShapeshiftForm();
        return form == FORM_TRAVEL || form == FORM_AQUA ||
               form == FORM_FLIGHT || form == FORM_FLIGHT_EPIC ||
               form == FORM_BEAR || form == FORM_DIREBEAR ||
               form == FORM_CAT;
    }
    else if (bot->getClass() == CLASS_PRIEST)
    {
        return bot->GetShapeshiftForm() == FORM_SPIRITOFREDEMPTION;
    }

    return false;
}
}

namespace BotConsumables
{
namespace
{
constexpr float SAFE_CONSUME_ENEMY_RANGE = 40.0f;

bool HasSeatedRegenAura(Player* bot, AuraType auraType)
{
    for (AuraEffect const* effect : bot->GetAuraEffectsByType(auraType))
        if (effect->GetSpellInfo()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
            return true;

    return false;
}
}

bool IsEatingFood(Player* bot)
{
    return HasSeatedRegenAura(bot, SPELL_AURA_MOD_REGEN) || HasSeatedRegenAura(bot, SPELL_AURA_OBS_MOD_HEALTH);
}

bool IsDrinking(Player* bot)
{
    return HasSeatedRegenAura(bot, SPELL_AURA_MOD_POWER_REGEN) || HasSeatedRegenAura(bot, SPELL_AURA_OBS_MOD_POWER);
}

bool IsSafeToConsumeInBattleground(PlayerbotAI* botAI, Player* bot)
{
    if (!bot->InBattleground())
        return true;

    Battleground* bg = bot->GetBattleground();
    if (!bg || bg->GetStatus() == STATUS_WAIT_JOIN || bg->GetStatus() == STATUS_WAIT_LEAVE)
        return true;

    GuidVector enemies = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (ObjectGuid const& guid : enemies)
    {
        Unit* enemy = botAI->GetUnit(guid);
        if (enemy && enemy->IsAlive() && bot->IsWithinDistInMap(enemy, SAFE_CONSUME_ENEMY_RANGE))
            return false;
    }

    return true;
}
}

bool DrinkAction::Execute(Event event)
{
    if (botAI->HasCheat(BotCheatMask::food))
    {
        // if (bot->IsNonMeleeSpellCast(true))
        //     return false;

        bot->ClearUnitState(UNIT_STATE_CHASE);
        bot->ClearUnitState(UNIT_STATE_FOLLOW);

        if (bot->isMoving())
        {
            bot->StopMoving();
            // botAI->SetNextCheckDelay(sPlayerbotAIConfig->globalCoolDown);
            // return false;
        }
        bot->SetStandState(UNIT_STAND_STATE_SIT);
        botAI->InterruptSpell();

        // float hp = bot->GetHealthPercent();
        float mp = bot->GetPowerPct(POWER_MANA);
        float p = mp;
        float delay;

        if (!bot->InBattleground())
            delay = 18000.0f * (100 - p) / 100.0f;
        else
            delay = 12000.0f * (100 - p) / 100.0f;

        botAI->SetNextCheckDelay(delay);

        bot->AddAura(25990, bot);
        return true;
        // return botAI->CastSpell(24707, bot);
    }

    return UseItemAction::Execute(event);
}

bool DrinkAction::isUseful()
{
    return UseItemAction::isUseful() && AI_VALUE2(bool, "has mana", "self target") &&
           AI_VALUE2(uint8, "mana", "self target") < 100 && !BotConsumables::IsDrinking(bot) &&
           BotConsumables::IsSafeToConsumeInBattleground(botAI, bot);
}

bool DrinkAction::isPossible()
{
    if (bot->IsInCombat() || bot->IsMounted() || IsDisallowedShapeshiftForm(bot))
        return false;

    if (bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) || bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG) ||
        bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL))
    {
        return false;
    }

    return botAI->HasCheat(BotCheatMask::food) || UseItemAction::isPossible();
}

bool ContinueEatingAction::Execute(Event event)
{
    if (!bot->IsSitState())
        bot->SetStandState(UNIT_STAND_STATE_SIT);

    // PlayerbotAI::DoNextAction force-stands a sitting bot once the check delay drops below
    // 1000ms, which breaks the regen aura — the hold must be at least that long.
    botAI->SetNextCheckDelay(1000);
    return true;
}

bool ContinueEatingAction::isUseful() { return BotConsumables::IsSafeToConsumeInBattleground(botAI, bot); }

bool EatAction::Execute(Event event)
{
    if (botAI->HasCheat(BotCheatMask::food))
    {
        // if (bot->IsNonMeleeSpellCast(true))
        //     return false;

        bot->ClearUnitState(UNIT_STATE_CHASE);
        bot->ClearUnitState(UNIT_STATE_FOLLOW);

        if (bot->isMoving())
        {
            bot->StopMoving();
            // botAI->SetNextCheckDelay(sPlayerbotAIConfig.globalCoolDown);
            // return false;
        }

        bot->SetStandState(UNIT_STAND_STATE_SIT);
        botAI->InterruptSpell();

        float hp = bot->GetHealthPct();
        // float mp = bot->HasMana() ? bot->GetPowerPercent() : 0.f;
        float p = hp;
        float delay;

        if (!bot->InBattleground())
            delay = 18000.0f * (100 - p) / 100.0f;
        else
            delay = 12000.0f * (100 - p) / 100.0f;

        botAI->SetNextCheckDelay(delay);

        bot->AddAura(25990, bot);
        return true;
    }

    return UseItemAction::Execute(event);
}

bool EatAction::isUseful()
{
    return UseItemAction::isUseful() && AI_VALUE2(uint8, "health", "self target") < 100 &&
           !BotConsumables::IsEatingFood(bot) && BotConsumables::IsSafeToConsumeInBattleground(botAI, bot);
}

bool EatAction::isPossible()
{
    if (bot->IsInCombat() || bot->IsMounted() || IsDisallowedShapeshiftForm(bot))
        return false;

    if (bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) || bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG) ||
        bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL))
    {
        return false;
    }

    return botAI->HasCheat(BotCheatMask::food) || UseItemAction::isPossible();
}
