#ifndef PLAYERBOTS_DISPELBACKOFF_H
#define PLAYERBOTS_DISPELBACKOFF_H

#include <mutex>
#include <unordered_map>
#include <vector>

#include "ObjectGuid.h"

class Player;
class Unit;

// A bot whose debuff keeps getting dispelled off a target should stop
// feeding it globals - reapplying into a Paladin's Cleanse forever is the
// tell of a rotation bot, and worse, the reapply triggers outrank the real
// damage rotation so the bot does nothing else (felworld/mod-playerbots#108).
// After AiPlayerbot.DebuffDispelBackoffCount dispels of the same dispel
// school (magic/curse/disease/poison) off the same target within
// AiPlayerbot.DebuffDispelBackoffSeconds, DebuffTrigger and
// CastDebuffSpellAction hold every debuff of that school against that
// target, letting the engine fall through to direct damage. The window
// slides, so the bot re-tests once things quiet down and re-holds
// immediately if the re-test gets cleansed too. Keying by school (not
// spell) is the point: two cleansed Shadow Word: Pains prove Vampiric
// Touch won't stick either, while a curse still lands on a Paladin who
// can't touch curses - the board learns each opponent's actual dispel
// coverage, in PvP and PvE alike, with no class table.
//
// True when `spellId` is a dispellable debuff whose school is currently
// held against `target` for this bot.
bool DispelBackoffActive(Player* bot, Unit* target, uint32 spellId);

// Recent enemy dispels of a bot's debuffs, keyed (caster, target, school).
// All state is mutex-guarded - the aura-remove hook and trigger checks run
// on different map-update threads.
class DispelBackoffBoard
{
public:
    static DispelBackoffBoard& instance()
    {
        static DispelBackoffBoard instance;
        return instance;
    }

    // From the aura-remove hook: an enemy dispelled `caster`'s debuff of
    // `dispelType` off `target`.
    void RecordDispel(Player* caster, Unit* target, uint8 dispelType);

    // The threshold was met inside the rolling window.
    bool IsBackedOff(ObjectGuid caster, ObjectGuid target, uint8 dispelType);

private:
    DispelBackoffBoard() = default;

    std::mutex _mutex;
    std::unordered_map<uint64, std::vector<uint32>> _dispelTimesMs;
};

#endif
