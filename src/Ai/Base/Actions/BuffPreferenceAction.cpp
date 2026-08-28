/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BuffPreferenceAction.h"

#include <sstream>

#include "BuffPreference.h"
#include "Event.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace
{
std::string SpellDisplayName(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !spellInfo->SpellName[0])
        return "that buff";

    return spellInfo->SpellName[0];
}
}  // namespace

bool BuffPreferenceAction::Execute(Event event)
{
    // Anyone may ask a bot to keep a particular buff on them, so the reply
    // has to reach a requester who is not this bot's master.
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    if (!requester)
        return false;

    std::string const param = event.getParam();
    std::ostringstream out;

    if (param.empty())
    {
        uint32 const preferred = BuffPreferenceBoard::instance().Get(bot, requester);
        if (preferred)
            out << "I am keeping " << SpellDisplayName(preferred) << " on you.";
        else
            out << "No buff preference on file for you - say 'prefer buff <name>' to set one.";
    }
    else if (param == "none" || param == "clear" || param == "off")
    {
        BuffPreferenceBoard::instance().Set(bot->GetGUID(), requester->GetGUID(), 0);
        out << "Forgotten - you get my usual buff from now on.";
    }
    else
    {
        // "spell id" resolves a name against the bot's own spellbook, so an
        // unknown or untrained buff is refused rather than silently stored.
        uint32 const spellId = AI_VALUE2(uint32, "spell id", param);
        if (!spellId)
        {
            out << "I do not know a spell called \"" << param << "\".";
        }
        else
        {
            uint32 const firstRank = sSpellMgr->GetFirstSpellInChain(spellId);
            BuffPreferenceBoard::instance().Set(bot->GetGUID(), requester->GetGUID(), firstRank);
            out << SpellDisplayName(firstRank) << " it is - I will keep that one on you.";
        }
    }

    if (requester == GetMaster())
        botAI->TellMasterNoFacing(out.str());
    else if (bot->IsInWorld() && requester->IsInWorld())
        bot->Whisper(out.str(), LANG_UNIVERSAL, requester);

    return true;
}
