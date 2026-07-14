/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_BYSTANDERTRIGGERS_H
#define _PLAYERBOT_BYSTANDERTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

// Fires when a nearby non-group player looks like they're about to die and
// this bot can legally and plausibly save them (see BystanderToAssistValue).
// checkInterval 1: a rescue window is a couple of seconds, so this must run
// on every non-combat tick - the value keeps the scan bounded.
class BystanderInDistressTrigger : public Trigger
{
public:
    BystanderInDistressTrigger(PlayerbotAI* botAI) : Trigger(botAI, "bystander in distress", 1) {}

    bool IsActive() override;
};

#endif
