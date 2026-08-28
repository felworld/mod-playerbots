/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BUFFPREFERENCEACTION_H
#define PLAYERBOTS_BUFFPREFERENCEACTION_H

#include "Action.h"

class PlayerbotAI;

// "prefer buff <name>" - the speaker tells the bot which buff they want kept
// on them when the bot would otherwise pick by role. "prefer buff none" takes
// it back, a bare "prefer buff" reports what is on file. The choice lasts as
// long as the party the two share.
class BuffPreferenceAction : public Action
{
public:
    BuffPreferenceAction(PlayerbotAI* botAI) : Action(botAI, "prefer buff") {}

    bool Execute(Event event) override;
};

#endif
