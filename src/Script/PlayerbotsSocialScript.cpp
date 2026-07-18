/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "Group.h"
#include "Player.h"
#include "Playerbots.h"
#include "ScriptMgr.h"
#include "SocialBuffValues.h"
#include "SpellAuras.h"
#include "SpellInfo.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
    // The classic long-duration class buffs (and their group variants) worth
    // reciprocating. Procs, food, weightstones and combat utility are not
    // social gestures.
    bool IsSocialBuffSpell(SpellInfo const* spellInfo)
    {
        static std::unordered_set<std::string> const names = {
            "arcane intellect", "arcane brilliance", "dalaran intellect", "dalaran brilliance",
            "power word: fortitude", "prayer of fortitude",
            "divine spirit", "prayer of spirit",
            "shadow protection", "prayer of shadow protection",
            "mark of the wild", "gift of the wild", "thorns",
            "blessing of might", "greater blessing of might",
            "blessing of wisdom", "greater blessing of wisdom",
            "blessing of kings", "greater blessing of kings",
            "blessing of sanctuary", "greater blessing of sanctuary",
        };

        if (!spellInfo->SpellName[0])
            return false;

        std::string name = spellInfo->SpellName[0];
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return names.find(name) != names.end();
    }

    // One reaction per bot/actor pair per cooldown window, so a generous
    // stranger doesn't get thanked (or rebuffed) on every renewal tick.
    // Guarded: the unit hooks fire on map-update threads.
    class PairCooldowns
    {
    public:
        bool TryStart(ObjectGuid bot, ObjectGuid actor, uint32 cooldownSecs)
        {
            uint64 key = bot.GetRawValue() ^ (actor.GetRawValue() << 1);
            uint32 now = getMSTime();

            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _endMs.find(key);
            if (it != _endMs.end() && now < it->second)
                return false;

            // Cheap unbounded-growth backstop; entries rebuild harmlessly.
            if (_endMs.size() > 8192)
                _endMs.clear();

            _endMs[key] = now + cooldownSecs * IN_MILLISECONDS;
            return true;
        }

    private:
        std::mutex _mutex;
        std::unordered_map<uint64, uint32> _endMs;
    };

    PairCooldowns buffBackCooldowns;
    PairCooldowns thankCooldowns;

    // Returns the bot's AI when `unit` is an idle, ungrouped playerbot that
    // the social-buff strategy actually runs for; nullptr otherwise.
    PlayerbotAI* EligibleBotAI(Unit* unit)
    {
        if (!unit || !unit->IsPlayer())
            return nullptr;

        Player* bot = unit->ToPlayer();
        if (bot->GetGroup())
            return nullptr;

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI || botAI->IsRealPlayer())
            return nullptr;

        return botAI;
    }
}

// Feeds the "social buff" strategy: when a friendly player buffs or heals a
// bot, stash who did it as a pending reaction (buff back, or /thank for
// heals) for the bot's AI loop to act on once it's out of combat.
class PlayerbotsSocialScript : public UnitScript
{
public:
    PlayerbotsSocialScript()
        : UnitScript("PlayerbotsSocialScript", true, { UNITHOOK_ON_AURA_APPLY, UNITHOOK_ON_HEAL })
    {
    }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        if (!sPlayerbotAIConfig.enableSocialBuffing)
            return;

        PlayerbotAI* botAI = EligibleBotAI(unit);
        if (!botAI)
            return;

        Unit* casterUnit = aura->GetCaster();
        Player* caster = casterUnit ? casterUnit->ToPlayer() : nullptr;
        if (!caster || caster == unit || !caster->IsFriendlyTo(unit))
            return;

        if (!IsSocialBuffSpell(aura->GetSpellInfo()))
            return;

        if (!buffBackCooldowns.TryStart(unit->GetGUID(), caster->GetGUID(), sPlayerbotAIConfig.socialBuffCooldown))
            return;

        botAI->GetAiObjectContext()->GetValue<SocialReactionEvent>("pending buff back")
            ->Set({ caster->GetGUID(), getMSTime() });
    }

    void OnHeal(Unit* healer, Unit* receiver, uint32& gain) override
    {
        if (!sPlayerbotAIConfig.enableHealThanks || !gain)
            return;

        if (!healer || healer == receiver || !healer->IsPlayer())
            return;

        PlayerbotAI* botAI = EligibleBotAI(receiver);
        if (!botAI)
            return;

        if (!thankCooldowns.TryStart(receiver->GetGUID(), healer->GetGUID(), sPlayerbotAIConfig.socialThankCooldown))
            return;

        botAI->GetAiObjectContext()->GetValue<SocialReactionEvent>("pending thank")
            ->Set({ healer->GetGUID(), getMSTime() });
    }
};

void AddPlayerbotsSocialScripts()
{
    new PlayerbotsSocialScript();
}
