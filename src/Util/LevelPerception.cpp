#include "LevelPerception.h"

#include <algorithm>

#include "Creature.h"
#include "Unit.h"

namespace
{
// World bosses wear the skull whatever the gap, so their level is never
// readable.
bool AlwaysSkull(Unit const* target)
{
    Creature const* creature = target->ToCreature();
    return creature && creature->isWorldBoss();
}
}  // namespace

bool IsLevelKnown(uint8 viewerLevel, uint8 targetLevel, bool hostile)
{
    if (!hostile)
        return true;

    return int32(targetLevel) - int32(viewerLevel) < int32(UNKNOWN_LEVEL_GAP);
}

bool IsLevelKnown(Unit const* viewer, Unit const* target)
{
    if (!viewer || !target)
        return false;

    if (AlwaysSkull(target))
        return false;

    // Anything the viewer isn't friendly toward counts as hostile here: an
    // unflagged enemy-faction player shows a skull just like a mob does.
    return IsLevelKnown(viewer->GetLevel(), target->GetLevel(), !viewer->IsFriendlyTo(target));
}

uint8 PerceivedLevel(uint8 viewerLevel, uint8 targetLevel, bool hostile)
{
    if (IsLevelKnown(viewerLevel, targetLevel, hostile))
        return targetLevel;

    // "??" says nothing but "at least this far above me", so that floor is all
    // the bot gets. It never exceeds the real level, so every "is this target
    // out of my league" test keeps answering the way it did.
    return std::min<uint8>(targetLevel, viewerLevel + UNKNOWN_LEVEL_GAP);
}

uint8 PerceivedLevel(Unit const* viewer, Unit const* target)
{
    if (!target)
        return 0;
    if (!viewer)
        return target->GetLevel();

    if (IsLevelKnown(viewer, target))
        return target->GetLevel();

    return std::min<uint8>(target->GetLevel(), viewer->GetLevel() + UNKNOWN_LEVEL_GAP);
}

std::string PerceivedLevelText(Unit const* viewer, Unit const* target)
{
    if (!target)
        return "??";

    return IsLevelKnown(viewer, target) ? std::to_string(target->GetLevel()) : "??";
}

std::string LevelPhrase(std::string const& perceivedLevelText)
{
    // "a level 28 mage", but "a ??-level mage" - the skull goes where players
    // put it rather than reading as the number it replaces.
    if (perceivedLevelText == "??")
        return "??-level";

    return "level " + perceivedLevelText;
}

std::string PerceivedLevelPhrase(Unit const* viewer, Unit const* target)
{
    return LevelPhrase(PerceivedLevelText(viewer, target));
}
