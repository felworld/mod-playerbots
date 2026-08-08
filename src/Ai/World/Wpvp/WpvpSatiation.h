#ifndef PLAYERBOTS_WPVPSATIATION_H
#define PLAYERBOTS_WPVPSATIATION_H

#include <mutex>
#include <unordered_map>

#include "ObjectGuid.h"

class Player;

// Killer-side counterpart to BotDeathSafety's victim-side camp patience:
// after each world-PvP kill the killing bot rolls
// AiPlayerbot.WpvpSatiationChance to be "satiated" with that victim - done
// with them for AiPlayerbot.WpvpSatiationMinutes. Target evaluation skips a
// satiated pair, so the bot stops initiating; fighting back is untouched
// (the PvP combat-ref path never consults this). Each kill re-rolls the
// same dice, so most bots move on after a kill or two while a rare few
// keep camping - griefers exist, they're just no longer the norm.
bool WpvpSatiated(Player* bot, Player* enemy);

// Directional (killer -> victim) satiation entries with an expiry. All
// state is mutex-guarded - the PvP-kill hook and target-evaluation loops
// run on different map-update threads.
class WpvpSatiationBoard
{
public:
    static WpvpSatiationBoard& instance()
    {
        static WpvpSatiationBoard instance;
        return instance;
    }

    // From the PVP-kill hook (world kills only, killer a bot): roll the
    // satiation dice and record the grace on success.
    void RecordKill(Player* killer, Player* victim);

    // The killer rolled "satiated" against this victim and the grace is
    // still running.
    bool IsSatiated(ObjectGuid killer, ObjectGuid victim);

private:
    WpvpSatiationBoard() = default;

    void Prune(uint32 now);

    std::mutex _mutex;
    std::unordered_map<uint64, uint32> _satiatedUntilMs;
};

#endif
