/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PLAYERBOTCOMMANDSCRIPT_H
#define PLAYERBOTS_PLAYERBOTCOMMANDSCRIPT_H

class ChatHandler;

// .playerbots enable|disable|status handlers, free functions (not class-local)
// so the unit tests (test/PlayerbotsToggleCommandTest.cpp) can call them directly.
bool HandlePlayerbotsEnableCommand(ChatHandler* handler, char const* args);
bool HandlePlayerbotsDisableCommand(ChatHandler* handler, char const* args);
bool HandlePlayerbotsStatusCommand(ChatHandler* handler, char const* args);

void AddPlayerbotsCommandscripts();

#endif
