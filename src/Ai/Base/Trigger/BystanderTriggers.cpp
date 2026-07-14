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

    if (bot->GetGroup() || bot->IsInCombat())
        return false;

    return AI_VALUE(Unit*, "bystander to assist") != nullptr;
}
