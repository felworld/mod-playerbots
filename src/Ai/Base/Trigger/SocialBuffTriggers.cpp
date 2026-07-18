/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "SocialBuffTriggers.h"

#include "Playerbots.h"
#include "SocialBuffValues.h"

bool PasserbyToBuffTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.enableSocialBuffing)
        return false;

    if (bot->GetGroup() || bot->IsInCombat())
        return false;

    return AI_VALUE(Unit*, "passerby to buff") != nullptr;
}

bool BuffedByFriendlyTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.enableSocialBuffing)
        return false;

    if (bot->IsInCombat())
        return false;

    return IsSocialReactionFresh(AI_VALUE(SocialReactionEvent, "pending buff back"), SOCIAL_BUFF_BACK_WINDOW_MS);
}

bool HealedByFriendlyTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.enableHealThanks)
        return false;

    if (bot->IsInCombat())
        return false;

    return IsSocialReactionFresh(AI_VALUE(SocialReactionEvent, "pending thank"), SOCIAL_THANK_WINDOW_MS);
}
