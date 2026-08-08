#ifndef PLAYERBOTS_LEVELPERCEPTION_H
#define PLAYERBOTS_LEVELPERCEPTION_H

#include <string>

#include "Define.h"

class Unit;

// Level perception (Felworld): the client hides a hostile unit's level once it
// is this far above the viewer - UnitLevel() returns -1 and the frame shows a
// skull, so all a player learns from "??" is "at least this many levels above
// me". Friendly units always show their real level.
constexpr uint8 UNKNOWN_LEVEL_GAP = 10;

// True when the viewer could read an exact number off the target's frame.
bool IsLevelKnown(uint8 viewerLevel, uint8 targetLevel, bool hostile);
bool IsLevelKnown(Unit const* viewer, Unit const* target);

// The level a bot may act on: the true one while it is readable, otherwise the
// floor the skull implies (viewer + UNKNOWN_LEVEL_GAP). Every comparison a bot
// makes against another unit's level goes through here, so it can never decide
// - or say - anything off a number a player at the same keyboard couldn't see.
uint8 PerceivedLevel(uint8 viewerLevel, uint8 targetLevel, bool hostile);
uint8 PerceivedLevel(Unit const* viewer, Unit const* target);

// The level as it would be spoken: the number, or "??" for a skull.
std::string PerceivedLevelText(Unit const* viewer, Unit const* target);

// The same, worded the way a player describes someone: "level 28", or
// "??-level" for a skull - the shorthand everyone uses for "no idea, high".
std::string LevelPhrase(std::string const& perceivedLevelText);
std::string PerceivedLevelPhrase(Unit const* viewer, Unit const* target);

#endif
