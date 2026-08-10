#include "WpvpAssist.h"

#include "CombatManager.h"
#include "Common.h"
#include "DeterministicRoll.h"
#include "LevelPerception.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "WpvpGrudge.h"

namespace
{
    // How much effective distance one perceived level of advantage is worth:
    // an enemy 5 levels below the bot reads as ~20yd closer than they are,
    // enough to out-pull a slightly nearer even match without teleporting the
    // bot's attention across the battlefield.
    constexpr float PULL_PER_LEVEL_BELOW = 4.0f;

    // Extra pull for an active combatant: someone already swinging at players
    // is a threat/target NOW, where an idle enemy is only a prospect.
    constexpr float ACTIVE_COMBATANT_PULL = 15.0f;

    // And the strongest pull of all for the bot's own recent killer: a
    // revenge grudge (WpvpGrudge) outranks every other attraction on the
    // field, so the rezzed bot goes for them, not the nearest stranger.
    constexpr float REVENGE_PULL = 30.0f;

    // Join dice hold for the same window as the attack-decision hash, so a
    // passerby that shrugged at a fight doesn't change its mind next tick.
    constexpr uint64 PASSERBY_ROLL_SALT = 0x50415353;  // 'PASS'
    constexpr uint32 PASSERBY_ROLL_WINDOW = 2 * MINUTE;
}

bool WpvpActivePvpCombatant(Player* candidate)
{
    if (!candidate)
        return false;

    for (auto const& [guid, combatRef] : candidate->GetCombatManager().GetPvPCombatRefs())
    {
        Unit* other = combatRef->GetOther(candidate);
        if (!other || !other->IsPlayer() || !other->IsAlive())
            continue;

        if (candidate->duel && candidate->duel->Opponent == other)
            continue;

        return true;
    }

    return false;
}

float WpvpTargetScore(Player* bot, Player* candidate)
{
    float score = bot->GetDistance(candidate);

    int32 levelsBelow = int32(bot->GetLevel()) - int32(PerceivedLevel(bot, candidate));
    if (levelsBelow > 0)
        score -= PULL_PER_LEVEL_BELOW * float(levelsBelow);

    if (WpvpActivePvpCombatant(candidate))
        score -= ACTIVE_COMBATANT_PULL;

    if (WpvpGrudgeAgainst(bot, candidate) == WpvpGrudgeDisposition::Revenge)
        score -= REVENGE_PULL;

    return score;
}

bool WpvpPasserbyAssistTarget(Player* bot, Player* enemy)
{
    if (!sPlayerbotAIConfig.wpvpPasserbyAssistEnabled || !bot || !enemy)
        return false;

    // Only a solo, already-flagged world passerby joins: etiquette never
    // demands flagging yourself, grouped bots answer to their party (the
    // party-assist phase covers them), and instanced PvP has its own rules.
    if (!bot->IsPvP() || bot->GetGroup() || bot->InBattleground() || bot->InArena())
        return false;

    float const radius = sPlayerbotAIConfig.wpvpPasserbyAssistRadius;
    if (bot->GetDistance(enemy) > radius)
        return false;

    // The massacre line: a skull on the enemy's frame means joining is dying
    // as the add, so the attack path stops here. Healer classes may still
    // support the victim through bystander assist.
    if (int32(PerceivedLevel(bot, enemy)) - int32(bot->GetLevel()) >= int32(UNKNOWN_LEVEL_GAP))
        return false;

    // The fight has to be on the bot's screen: the enemy is trading blows
    // with a living faction-mate inside the same radius. Duels don't count -
    // nobody "helps" a duel.
    bool fightingAllyNearby = false;
    for (auto const& [guid, combatRef] : enemy->GetCombatManager().GetPvPCombatRefs())
    {
        Unit* other = combatRef->GetOther(enemy);
        if (!other || !other->IsPlayer() || !other->IsAlive())
            continue;

        Player* ally = other->ToPlayer();
        if (ally == bot || ally->GetTeamId() != bot->GetTeamId())
            continue;

        if (ally->duel && ally->duel->Opponent == enemy)
            continue;

        if (bot->GetDistance(ally) > radius)
            continue;

        fightingAllyNearby = true;
        break;
    }

    if (!fightingAllyNearby)
        return false;

    return DeterministicRollPasses(bot->GetGUID().GetRawValue(), enemy->GetGUID().GetRawValue(),
                                   PASSERBY_ROLL_SALT, PASSERBY_ROLL_WINDOW,
                                   sPlayerbotAIConfig.wpvpPasserbyAssistChance);
}
