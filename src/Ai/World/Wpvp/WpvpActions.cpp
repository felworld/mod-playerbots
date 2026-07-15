#include "WpvpActions.h"

#include "NewRpgInfo.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Random.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "WpvpTriggers.h"

bool WpvpGoadAction::Execute(Event /*event*/)
{
    auto* data = std::get_if<NewRpgInfo::GoWpvp>(&botAI->rpgInfo.data);
    if (!data)
        return false;

    Unit* mark = nullptr;
    GuidVector targets = AI_VALUE(GuidVector, "nearest unflagged enemy players");
    for (ObjectGuid const guid : targets)
    {
        Unit* enemy = botAI->GetUnit(guid);
        if (enemy && enemy->IsAlive() && bot->GetDistance(enemy) < WpvpGoadTrigger::GOAD_RANGE)
        {
            mark = enemy;
            break;
        }
    }
    if (!mark)
        return false;

    botAI->RemoveAura("stealth");
    botAI->RemoveAura("prowl");
    bot->SetFacingToObject(mark);

    static constexpr Emote provocations[] = {EMOTE_ONESHOT_RUDE, EMOTE_ONESHOT_CHICKEN, EMOTE_ONESHOT_POINT};
    bot->HandleEmoteCommand(provocations[urand(0, 2)]);

    data->lastGoadT = getMSTime();
    return true;
}
