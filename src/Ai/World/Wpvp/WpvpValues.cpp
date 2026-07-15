#include "WpvpValues.h"

#include "Player.h"
#include "Playerbots.h"

bool NearestUnflaggedEnemyPlayersValue::AcceptUnit(Unit* unit)
{
    if (!PossibleTargetsValue::AcceptUnit(unit))
        return false;

    Player* enemy = unit->ToPlayer();
    return enemy && botAI->IsOpposing(enemy) && !enemy->IsPvP() &&
           !sPlayerbotAIConfig.IsPvpProhibited(enemy->GetZoneId(), enemy->GetAreaId());
}
