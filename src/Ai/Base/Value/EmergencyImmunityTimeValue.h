/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EMERGENCYIMMUNITYTIMEVALUE_H
#define PLAYERBOTS_EMERGENCYIMMUNITYTIMEVALUE_H

#include "Value.h"

class PlayerbotAI;

// When the bot last cast one of its own immunities (Ice Block, Divine Shield, Dispersion) as a
// survival move - at low health - rather than as a scripted mechanic dodge at full health. Only a
// survival cast is dropped early once the bot has been healed back up (Felworld).
class EmergencyImmunityTimeValue : public ManualSetValue<time_t>
{
public:
    EmergencyImmunityTimeValue(PlayerbotAI* botAI) : ManualSetValue<time_t>(botAI, 0, "emergency immunity time") {}
};

#endif
