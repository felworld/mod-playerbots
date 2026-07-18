/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_SOCIALBUFFTRIGGERS_H
#define _PLAYERBOT_SOCIALBUFFTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

class PasserbyToBuffTrigger : public Trigger
{
public:
    PasserbyToBuffTrigger(PlayerbotAI* botAI) : Trigger(botAI, "passerby to buff", 2) {}

    bool IsActive() override;
};

class BuffedByFriendlyTrigger : public Trigger
{
public:
    BuffedByFriendlyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "buffed by friendly", 1) {}

    bool IsActive() override;
};

class HealedByFriendlyTrigger : public Trigger
{
public:
    HealedByFriendlyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "healed by friendly", 1) {}

    bool IsActive() override;
};

#endif
