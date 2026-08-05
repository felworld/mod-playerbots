#include "WpvpValues.h"

#include "Player.h"
#include "Playerbots.h"
#include "WpvpGuardRespect.h"

bool NearestUnflaggedEnemyPlayersValue::AcceptUnit(Unit* unit)
{
    // Deliberately NOT routed through PossibleTargetsValue::AcceptUnit: the
    // AttackersValue filter it applies rejects every unflagged player, and
    // unflagged players are exactly what this value is for.
    Player* enemy = unit->ToPlayer();
    if (!enemy || !botAI->IsOpposing(enemy) || enemy->IsPvP() || enemy->IsFFAPvP())
        return false;

    if (!enemy->IsAlive() || !enemy->IsVisible() || !bot->CanSeeOrDetect(enemy) ||
        enemy->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
        return false;

    // Never provoke where the ensuing fight couldn't happen (either side in a
    // guarded neutral hub or other PvP-prohibited spot).
    if (sPlayerbotAIConfig.IsPvpProhibited(enemy->GetZoneId(), enemy->GetAreaId()) ||
        sPlayerbotAIConfig.IsPvpProhibited(bot->GetZoneId(), bot->GetAreaId()))
        return false;

    // Don't goad someone under their guards' cover either - the fight the
    // goad invites is one the guard bar would immediately break off.
    if (WpvpGuardsBarPursuit(bot, enemy))
        return false;

    // Don't goad someone the bot wouldn't actually fight (PossibleTargetsValue
    // refuses targets 5+ levels above the bot).
    return int32(enemy->GetLevel()) - int32(bot->GetLevel()) < 5;
}
