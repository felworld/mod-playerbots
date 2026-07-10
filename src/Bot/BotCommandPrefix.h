/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_BOTCOMMANDPREFIX_H
#define _PLAYERBOT_BOTCOMMANDPREFIX_H

#include <string>

// Applies AiPlayerbot.CommandPrefix to an incoming chat message: with a prefix
// configured, only messages starting with it are bot commands - everything else
// is ordinary chat (so "who said that?" gets a reply instead of being swallowed
// by the "who" command). Strips the prefix from text and returns true when the
// message is a command; an empty prefix treats every message as a command.
inline bool StripBotCommandPrefix(std::string& text, std::string const& prefix)
{
    if (prefix.empty())
        return true;

    if (text.rfind(prefix, 0) != 0)
        return false;

    text.erase(0, prefix.size());
    return true;
}

#endif
