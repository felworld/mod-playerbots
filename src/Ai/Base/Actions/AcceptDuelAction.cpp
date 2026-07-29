/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AcceptDuelAction.h"

#include "Event.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"
#include "WpvpDefense.h"

bool AcceptDuelAction::Execute(Event event)
{
    WorldPacket p(event.getPacket());

    ObjectGuid flagGuid;
    p >> flagGuid;
    ObjectGuid playerGuid;
    p >> playerGuid;

    // do not auto duel with low hp
    if ((!botAI->HasRealPlayerMaster() || (botAI->GetMaster() && botAI->GetMaster()->GetGUID() != playerGuid)) &&
        AI_VALUE2(uint8, "health", "self target") < 90)
    {
        WorldPacket packet(CMSG_DUEL_CANCELLED, 8);
        packet << flagGuid;
        bot->GetSession()->HandleDuelCancelledOpcode(packet);
        return false;
    }

    // A bot's challenge issued moments before world PvP broke out nearby is
    // declined - the initiation gate has the same check, this closes the
    // race. A real player's request is honored: a human read the room.
    Player* requester = ObjectAccessor::FindPlayer(playerGuid);
    if (requester && GET_PLAYERBOT_AI(requester) && WpvpHappeningNearby(bot))
    {
        WorldPacket packet(CMSG_DUEL_CANCELLED, 8);
        packet << flagGuid;
        bot->GetSession()->HandleDuelCancelledOpcode(packet);
        return false;
    }

    WorldPacket packet(CMSG_DUEL_ACCEPTED, 8);
    packet << flagGuid;
    bot->GetSession()->HandleDuelAcceptedOpcode(packet);

    botAI->ResetStrategies();
    return true;
}
