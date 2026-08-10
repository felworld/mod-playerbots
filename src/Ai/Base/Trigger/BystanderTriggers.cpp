/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "BystanderTriggers.h"

#include "Playerbots.h"

bool BystanderInDistressTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.enableBystanderAssist)
        return false;

    if (bot->GetGroup())
        return false;

    // No in-combat gate here: the value adopts victims only out of combat,
    // but a healer mid-rescue keeps returning the one it adopted (sustain),
    // and this trigger runs in the combat engine too so the rescue survives
    // the first support heal pulling the healer into combat.
    return AI_VALUE(Unit*, "bystander to assist") != nullptr;
}
