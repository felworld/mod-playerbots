/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GenericTriggers.h"
#include "BattlegroundWS.h"
#include "BotDeathSafety.h"
#include "CcTargetValue.h"
#include "Corpse.h"
#include "CraftBandageAction.h"
#include "CreatureAI.h"
#include "DungeonHoldValues.h"
#include "EngineeringDeviceActions.h"
#include "EngineeringTinkerActions.h"
#include "GenericBuffUtils.h"
#include "NonCombatActions.h"
#include "ThrowExplosivesAction.h"
#include "ItemVisitors.h"
#include "LastSpellCastValue.h"
#include "LevelPerception.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "SharedDefines.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "Timer.h"
#include <string>

bool LowManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") &&
           AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.lowMana;
}

bool MediumManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") &&
           AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.mediumMana;
}

bool LowEnergyTrigger::IsActive()
{
    return AI_VALUE2(uint8, "energy", "self target") < threshold;
}

bool NoPetTrigger::IsActive()
{
    return bot->GetMinionGUID().IsEmpty() && !AI_VALUE(Unit*, "pet target") && !bot->GetGuardianPet() &&
           !bot->GetFirstControlled() && !AI_VALUE2(bool, "mounted", "self target");
}

bool HasPetTrigger::IsActive()
{
    return AI_VALUE(Unit*, "pet target") && !AI_VALUE2(bool, "mounted", "self target");
}

bool PetAttackTrigger::IsActive()
{
    Guardian* pet = bot->GetGuardianPet();
    if (!pet || !pet->IsAlive())
        return false;

    // Uncontrollable guardians have no CharmInfo and cannot be commanded at all.
    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
        return false;

    // A passive pet is either configured that way or parked by PullStrategy - do not fight it.
    if (pet->GetReactState() == REACT_PASSIVE)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    if (pet->GetVictim() == target && charmInfo->IsCommandAttack())
        return false;

    // Assist only: in an instance with a group the pet never starts a fight, and it waits out the
    // dungeon hold on top of that (Felworld).
    if (IsInstancedGroupContent(bot) && (!target->IsInCombat() || ShouldHoldForTank(botAI, target)))
        return false;

    return true;
}

bool DungeonHoldReleaseTrigger::IsActive()
{
    // Deliberately not IsDungeonHoldActive: a main tank that dies mid-window releases the hold, and
    // the bots it was holding still need to be told to start swinging.
    if (!sPlayerbotAIConfig.dungeonHoldForTank || !IsInstancedGroupContent(bot))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || bot->GetVictim() == target)
        return false;

    if (!bot->IsValidAttackTarget(target))
        return false;

    return !ShouldHoldForTank(botAI, target);
}

bool PetHoldReleaseTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.dungeonHoldForTank || !IsInstancedGroupContent(bot))
        return false;

    Guardian* pet = bot->GetGuardianPet();
    if (!pet || !pet->IsAlive())
        return false;

    // Uncontrollable guardians have no CharmInfo and cannot be commanded at all.
    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
        return false;

    // A passive pet is either configured that way or parked by PullStrategy - do not fight it.
    if (pet->GetReactState() == REACT_PASSIVE)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || !target->IsInCombat())
        return false;

    if (pet->GetVictim() == target && charmInfo->IsCommandAttack())
        return false;

    if (!bot->IsValidAttackTarget(target))
        return false;

    return !ShouldHoldForTank(botAI, target);
}

bool HighManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") &&
           AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.highMana;
}

bool AlmostFullManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") &&
           AI_VALUE2(uint8, "mana", "self target") > 85;
}

bool EnoughManaTrigger::IsActive()
{
    return AI_VALUE2(bool, "has mana", "self target") &&
           AI_VALUE2(uint8, "mana", "self target") > sPlayerbotAIConfig.highMana;
}

bool RageAvailable::IsActive() { return AI_VALUE2(uint8, "rage", "self target") >= amount; }

bool EnergyAvailable::IsActive() { return AI_VALUE2(uint8, "energy", "self target") >= amount; }

bool ComboPointsAvailableTrigger::IsActive() { return AI_VALUE2(uint8, "combo", "current target") >= amount; }

bool ComboPointsNotFullTrigger::IsActive() { return AI_VALUE2(uint8, "combo", "current target") < amount; }

bool TargetWithComboPointsLowerHealTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    return ComboPointsAvailableTrigger::IsActive() &&
           (target->GetHealth() / AI_VALUE(float, "estimated group dps")) <= lifeTime;
}

bool LoseAggroTrigger::IsActive() { return !AI_VALUE2(bool, "has aggro", "current target"); }

bool HasAggroTrigger::IsActive() { return AI_VALUE2(bool, "has aggro", "current target"); }

bool PanicTrigger::IsActive()
{
    return AI_VALUE2(uint8, "health", "self target") < sPlayerbotAIConfig.criticalHealth &&
           (!AI_VALUE2(bool, "has mana", "self target") ||
            AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.lowMana);
}

bool OutNumberedTrigger::IsActive()
{
    if (bot->GetMap() && (bot->GetMap()->IsDungeon() || bot->GetMap()->IsRaid()))
        return false;

    if (bot->GetGroup() && bot->GetGroup()->isRaidGroup())
        return false;

    int32 botLevel = bot->GetLevel();
    uint32 friendPower = 200;
    uint32 foePower = 0;
    for (auto& attacker : botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get())
    {
        Creature* creature = botAI->GetCreature(attacker);
        if (!creature)
            continue;

        int32 dLevel = PerceivedLevel(bot, creature) - botLevel;
        if (dLevel > -10)
            foePower = std::max(100 + 10 * dLevel, dLevel * 200);
    }

    if (!foePower)
        return false;

    for (auto& helper : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest friendly players")->Get())
    {
        Unit* player = botAI->GetUnit(helper);
        if (!player || player == bot)
            continue;

        int32 dLevel = player->GetLevel() - botLevel;

        if (dLevel > -10 && bot->GetDistance(player) < 10.0f)
            friendPower += std::max(200 + 20 * dLevel, dLevel * 200);
    }

    return friendPower < foePower;
}

bool BuffTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    Aura* aura = botAI->GetAura(spell, target, checkIsOwner, checkDuration);
    if (!aura || (beforeDuration && uint32(aura->GetDuration()) < beforeDuration))
        return true;

    return false;
}

Value<Unit*>* BuffOnPartyTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>(
        "party member without aura", ai::buff::MakeAuraQualifierForBuff(spell));
}

bool BuffOnPartyTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (ai::buff::ShouldDeferPartyBuffEvaluationForRecentLogin(bot, target, spell))
        return false;

    return BuffTrigger::IsActive();
}

bool ProtectPartyMemberTrigger::IsActive() { return AI_VALUE(Unit*, "party member to protect"); }

Value<Unit*>* DebuffOnAttackerTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("attacker without aura", spell);
}

Value<Unit*>* DebuffOnMeleeAttackerTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("melee attacker without aura", spell);
}

bool NoAttackersTrigger::IsActive()
{
    return !AI_VALUE(Unit*, "current target") && AI_VALUE(uint8, "my attacker count") > 0;
}

bool InvalidTargetTrigger::IsActive() { return AI_VALUE2(bool, "invalid target", "current target"); }

bool NoTargetTrigger::IsActive() { return !AI_VALUE(Unit*, "current target"); }

bool MyAttackerCountTrigger::IsActive()
{
    return AI_VALUE2(bool, "combat", "self target") && AI_VALUE(uint8, "my attacker count") >= amount;
}

bool MediumThreatTrigger::IsActive()
{
    if (!AI_VALUE2(bool, "combat", "self target"))
        return false;

    int32 count = 0;
    for (Unit* attacker : bot->getAttackers())
    {
        if (attacker->IsControlledByPlayer())
            continue;

        // Dropping threat only helps if the creature has someone else to turn to
        if (attacker->GetThreatMgr().GetThreatListSize() < 2)
            continue;

        ++count;
    }

    return count >= amount;
}

bool PvpEscapeTrigger::IsActive()
{
    if (!bot->IsInCombat())
        return false;

    uint32 players = 0;
    for (Unit* attacker : bot->getAttackers())
        if (attacker->IsControlledByPlayer())
            ++players;

    if (!players)
        return false;

    if (players >= 2)
        return true;

    if (bot->GetHealthPct() >= sPlayerbotAIConfig.lowHealth)
        return false;

    // Behind on health, but the one opponent is closer to dropping: finish them instead
    Unit* target = AI_VALUE(Unit*, "current target");
    return !target || target->GetHealthPct() >= bot->GetHealthPct();
}

bool LowTankThreatTrigger::IsActive()
{
    Unit* mainTank = AI_VALUE(Unit*, "main tank");
    if (!mainTank)
        return false;

    Unit* current_target = AI_VALUE(Unit*, "current target");
    if (!current_target)
        return false;

    ThreatManager& mgr = current_target->GetThreatMgr();
    float threat = mgr.GetThreat(bot);
    float tankThreat = mgr.GetThreat(mainTank);
    return tankThreat == 0.0f || threat > tankThreat * 0.5f;
}

bool AoeTrigger::IsActive()
{
    Unit* current_target = AI_VALUE(Unit*, "current target");
    if (!current_target)
        return false;

    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    int attackers_count = 0;
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetDistance(current_target->GetPosition()) <= range)
            attackers_count++;
    }
    return attackers_count >= amount;
}

bool NoFoodTrigger::IsActive()
{
    bool isRandomBot = sRandomPlayerbotMgr.IsRandomBot(bot);
    if (isRandomBot && botAI->HasCheat(BotCheatMask::food))
        return false;

    return AI_VALUE2(std::vector<Item*>, "inventory items", "conjured food").empty();
}

bool NoDrinkTrigger::IsActive()
{
    bool isRandomBot = sRandomPlayerbotMgr.IsRandomBot(bot);
    if (isRandomBot && botAI->HasCheat(BotCheatMask::food))
        return false;

    return AI_VALUE2(std::vector<Item*>, "inventory items", "conjured water").empty();
}

bool ConsumingFoodOrDrinkTrigger::IsActive()
{
    if (bot->IsInCombat())
        return false;

    return (BotConsumables::IsEatingFood(bot) && bot->GetHealthPct() < 100.0f) ||
           (BotConsumables::IsDrinking(bot) && bot->GetPowerPct(POWER_MANA) < 100.0f);
}

namespace
{
// "Master is resting" tuning (Felworld). Internal - deliberately not configurable, like the dungeon
// spread constants in Formations.cpp.
constexpr float MASTER_REST_RANGE = 20.0f;         // close enough that his break is the bot's break
constexpr float MASTER_REST_TOPUP_PCT = 95.0f;     // worth a drink even if the usual low bar is not hit
constexpr uint32 MASTER_REST_STAGGER_MS = 4000;    // widest personal delay before taking the hint

// Each bot answers the master sitting down on its own beat instead of the whole party dropping at
// once. Deterministic per bot, so the same bot is always the eager one or the slow one.
uint32 MasterRestReactionDelay(Player* bot)
{
    return uint32((bot->GetGUID().GetRawValue() * 2654435761ULL) % MASTER_REST_STAGGER_MS);
}
}  // namespace

bool MasterIsRestingTrigger::IsActive()
{
    Player* master = botAI->GetMaster();
    if (!master || master == bot || !master->IsInWorld() || !master->IsSitState() ||
        master->GetMapId() != bot->GetMapId() || bot->GetExactDist(master) > MASTER_REST_RANGE)
    {
        masterSatDownAt = 0;
        return false;
    }

    if (!bot->IsAlive() || bot->IsInCombat() || master->IsInCombat())
        return false;

    uint32 const now = getMSTime();
    if (!masterSatDownAt)
        masterSatDownAt = now;

    if (getMSTimeDiff(masterSatDownAt, now) < MasterRestReactionDelay(bot))
        return false;

    if (bot->getPowerType() == POWER_MANA && bot->GetPowerPct(POWER_MANA) < MASTER_REST_TOPUP_PCT)
        return true;

    return bot->GetHealthPct() < MASTER_REST_TOPUP_PCT;
}

bool SelfResurrectTrigger::IsActive()
{
    // Once the enemy-wait hold expires, use the self-res even with the enemy still
    // nearby - better than releasing and losing it.
    return !bot->IsAlive() && bot->GetUInt32Value(PLAYER_SELF_RES_SPELL) &&
           (!BotDeathSafety::EnemyPlayerNear(bot) ||
            BotDeathSafety::TimeSinceDeath(bot) >= BotDeathSafety::SELF_RES_WAIT_SECONDS);
}

bool TargetInSightTrigger::IsActive() { return AI_VALUE(Unit*, "grind target"); }

bool DebuffTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    return BuffTrigger::IsActive() &&
           (target->GetHealth() / AI_VALUE(float, "estimated group dps")) >= needLifeTime;
}

bool DebuffOnBossTrigger::IsActive()
{
    if (!DebuffTrigger::IsActive())
        return false;

    Creature* creature = GetTarget()->ToCreature();
    return creature && (creature->IsDungeonBoss() || creature->isWorldBoss());
}

bool SpellTrigger::IsActive() { return GetTarget(); }

bool SpellCanBeCastTrigger::IsActive()
{
    Unit* target = GetTarget();
    return target && botAI->CanCastSpell(spell, target);
}

bool SpellNoCooldownTrigger::IsActive()
{
    uint32 spellId = AI_VALUE2(uint32, "spell id", name);
    if (!spellId)
        return false;

    return !bot->HasSpellCooldown(spellId);
}

bool SpellCooldownTrigger::IsActive()
{
    uint32 spellId = AI_VALUE2(uint32, "spell id", name);
    if (!spellId)
        return false;

    return bot->HasSpellCooldown(spellId);
}

RandomTrigger::RandomTrigger(PlayerbotAI* botAI, std::string const name, int32 probability)
    : Trigger(botAI, name), probability(probability), lastCheck(getMSTime()) {}

bool RandomTrigger::IsActive()
{
    if (getMSTime() - lastCheck < sPlayerbotAIConfig.repeatDelay)
        return false;

    lastCheck = getMSTime();
    int32 k = (int32)(probability / sPlayerbotAIConfig.randomChangeMultiplier);
    if (k < 1)
        k = 1;

    return (rand() % k) == 0;
}

bool AndTrigger::IsActive() { return ls && rs && ls->IsActive() && rs->IsActive(); }

std::string const AndTrigger::getName()
{
    std::string name(ls->getName());
    name = name + " and ";
    name = name + rs->getName();
    return name;
}

bool TwoTriggers::IsActive()
{
    if (name1.empty() || name2.empty())
        return false;

    Trigger* trigger1 = botAI->GetAiObjectContext()->GetTrigger(name1);
    Trigger* trigger2 = botAI->GetAiObjectContext()->GetTrigger(name2);

    if (!trigger1 || !trigger2)
        return false;

    return trigger1->IsActive() && trigger2->IsActive();
}

std::string const TwoTriggers::getName()
{
    std::string name;
    name = name1 + " and " + name2;
    return name;
}

namespace
{
// A player opponent justifies burst cooldowns on their own - unless they are already below
// AiPlayerbot.LowHealth, where the rotation finishes them and a long cooldown would be spent on a
// kill the bot gets anyway.
bool IsPlayerWorthBoost(Unit* target)
{
    return target && target->IsPlayer() && target->GetHealthPct() >= sPlayerbotAIConfig.lowHealth;
}
}  // namespace

bool BoostTrigger::IsActive()
{
    if (!BuffTrigger::IsActive())
        return false;

    if (IsPlayerWorthBoost(AI_VALUE(Unit*, "current target")))
        return true;

    return AI_VALUE(uint8, "balance") <= balance;
}

bool GenericBoostTrigger::IsActive()
{
    if (IsPlayerWorthBoost(AI_VALUE(Unit*, "current target")))
        return true;

    return AI_VALUE(uint8, "balance") <= balance;
}

bool HealerShouldAttackTrigger::IsActive()
{
    if (botAI->GetNearGroupMemberCount(sPlayerbotAIConfig.sightDistance) <= 1)
        return true;

    if (AI_VALUE2(uint8, "health", "party member to heal") < sPlayerbotAIConfig.almostFullHealth)
        return false;

    if (bot->GetAura(33891)) // Tree of Life
    {
        LastSpellCast& lastSpell = botAI->GetAiObjectContext()->GetValue<LastSpellCast&>("last spell cast")->Get();
        if (lastSpell.timer + 5 > time(nullptr))
            return false;
    }

    int manaThreshold;
    int balance = AI_VALUE(uint8, "balance");
    if (balance <= 50)
        manaThreshold = 85;
    else if (balance <= 100)
        manaThreshold = sPlayerbotAIConfig.highMana;
    else
        manaThreshold = sPlayerbotAIConfig.mediumMana;

    if (AI_VALUE2(bool, "has mana", "self target") && AI_VALUE2(uint8, "mana", "self target") < manaThreshold)
        return false;

    return true;
}

bool ItemCountTrigger::IsActive() { return AI_VALUE2(uint32, "item count", item) < uint32(count); }

bool InterruptSpellTrigger::IsActive()
{
    return SpellTrigger::IsActive() && botAI->IsInterruptableSpellCasting(GetTarget(), getName());
}

bool DeflectSpellTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsNonMeleeSpellCast(true) || target->GetTarget() != bot->GetGUID())
        return false;

    uint32 spellid = context->GetValue<uint32>("spell id", spell)->Get();
    if (!spellid)
        return false;

    SpellInfo const* deflectSpell = sSpellMgr->GetSpellInfo(spellid);
    if (!deflectSpell)
        return false;

    // warrior deflects all
    if (spell == "spell reflection")
        return true;

    // human priest feedback
    if (spell == "feedback")
        return true;

    SpellSchoolMask deflectSchool = SpellSchoolMask(deflectSpell->Effects[EFFECT_0].MiscValue);
    SpellSchoolMask attackSchool = SPELL_SCHOOL_MASK_NONE;

    if (Spell* spell = target->GetCurrentSpell(CURRENT_GENERIC_SPELL))
    {
        if (SpellInfo const* tarSpellInfo = spell->GetSpellInfo())
        {
            attackSchool = tarSpellInfo->GetSchoolMask();
            if (deflectSchool == attackSchool)
                return true;
        }
    }

    return false;
}

bool AttackerCountTrigger::IsActive() { return AI_VALUE(uint8, "attacker count") >= amount; }

bool HasAuraTrigger::IsActive() { return botAI->HasAura(getName(), GetTarget(), false, false, -1, true); }

bool LossOfControlTrigger::IsActive()
{
    return bot->HasAuraType(SPELL_AURA_MOD_STUN) ||
           bot->HasAuraType(SPELL_AURA_MOD_FEAR) ||
           bot->HasAuraType(SPELL_AURA_MOD_ROOT) ||
           bot->HasAuraType(SPELL_AURA_MOD_CONFUSE) ||
           bot->HasAuraType(SPELL_AURA_MOD_CHARM);
}

bool FearCharmSleepTrigger::IsActive()
{
    return bot->HasAuraType(SPELL_AURA_MOD_FEAR) ||
           bot->HasAuraType(SPELL_AURA_MOD_CHARM) ||
           bot->HasAuraType(SPELL_AURA_AOE_CHARM) ||
           bot->HasAuraWithMechanic(1 << MECHANIC_SLEEP);
}

bool FearSleepSapTrigger::IsActive()
{
    return bot->HasAuraType(SPELL_AURA_MOD_FEAR) ||
           bot->HasAuraWithMechanic(1 << MECHANIC_SLEEP) ||
           bot->HasAuraWithMechanic(1 << MECHANIC_SAPPED);
}

bool PoisonDiseaseBleedTrigger::IsActive()
{
    return botAI->HasAuraToDispel(bot, DISPEL_POISON, false) ||
           botAI->HasAuraToDispel(bot, DISPEL_DISEASE, false) ||
           bot->HasAuraWithMechanic(1 << MECHANIC_BLEED);
}

bool MovementImpairedTrigger::IsActive()
{
    return botAI->IsMovementImpaired(bot) &&
           !botAI->HasAnyAuraOf(bot, "stealth", "prowl", nullptr);
}

bool HasAuraStackTrigger::IsActive()
{
    return botAI->GetAura(getName(), GetTarget(), false, true, stack);
}

bool TimerTrigger::IsActive()
{
    time_t now = time(nullptr);

    if (now != lastCheck)
    {
        lastCheck = now;
        return true;
    }

    return false;
}

bool TimerBGTrigger::IsActive()
{
    time_t now = time(nullptr);

    if (now - lastCheck >= 60)
    {
        lastCheck = now;
        return true;
    }

    return false;
}

bool HasNoAuraTrigger::IsActive() { return !botAI->HasAura(getName(), GetTarget()); }

bool TankAssistTrigger::IsActive()
{
    if (!AI_VALUE(uint8, "attacker count"))
        return false;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    if (!currentTarget)
        return true;

    Unit* tankTarget = AI_VALUE(Unit*, "tank target");
    if (!tankTarget || currentTarget == tankTarget)
        return false;

    return AI_VALUE2(bool, "has aggro", "current target");
}

bool IsBehindTargetTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && AI_VALUE2(bool, "behind", "current target");
}

bool IsNotBehindTargetTrigger::IsActive()
{
    if (botAI->HasStrategy("stay", botAI->GetState()))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && !AI_VALUE2(bool, "behind", "current target");
}

bool IsNotFacingTargetTrigger::IsActive()
{
    if (botAI->HasStrategy("stay", botAI->GetState()))
        return false;

    return !AI_VALUE2(bool, "facing", "current target");
}

// The recast side of the loop players see as a mage re-sheeping a mob the group is burning: the
// trigger refires the instant the control breaks, so it also has to ask whether the mob is still
// worth controlling and whether this bot has budget left on it (Felworld).
bool HasCcTargetTrigger::IsActive()
{
    if (AI_VALUE2(Unit*, "current cc target", getName()))
        return false;

    Unit* target = AI_VALUE2(Unit*, "cc target", getName());
    return target && IsWorthCrowdControlling(botAI, target);
}

bool MovingFillerTrigger::IsActive()
{
    if (!bot->isMoving())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && target->IsAlive();
}

bool NoMovementTrigger::IsActive() { return !AI_VALUE2(bool, "moving", "self target"); }

bool NoPossibleTargetsTrigger::IsActive()
{
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    return !targets.size();
}

bool PossibleAddsTrigger::IsActive()
{
    return AI_VALUE(bool, "possible adds") && !AI_VALUE(ObjectGuid, "pull target");
}

bool NotDpsTargetActiveTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive())
    {
        Unit* enemy = AI_VALUE(Unit*, "enemy player target");
        if (target == enemy)
            return false;
    }

    Unit* dps = AI_VALUE(Unit*, "dps target");
    return dps && target != dps;
}

bool NotDpsAoeTargetActiveTrigger::IsActive()
{
    Unit* dps = AI_VALUE(Unit*, "dps aoe target");
    Unit* target = AI_VALUE(Unit*, "current target");
    Unit* enemy = AI_VALUE(Unit*, "enemy player target");

    if (target && target == enemy && target->IsAlive())
        return false;

    return dps && target != dps;
}

bool IsSwimmingTrigger::IsActive() { return AI_VALUE2(bool, "swimming", "self target"); }

bool HasNearestAddsTrigger::IsActive()
{
    GuidVector targets = AI_VALUE(GuidVector, "nearest adds");
    return targets.size();
}

bool HasItemForSpellTrigger::IsActive()
{
    std::string const spell = getName();
    uint32 spellId = AI_VALUE2(uint32, "spell id", spell);
    return spellId && AI_VALUE2(Item*, "item for spell", spellId);
}

bool TargetChangedTrigger::IsActive()
{
    Unit* oldTarget = context->GetValue<Unit*>("old target")->Get();
    Unit* target = context->GetValue<Unit*>("current target")->Get();
    return target && oldTarget != target;
}

Value<Unit*>* InterruptEnemyHealerTrigger::GetTargetValue()
{
    return context->GetValue<Unit*>("enemy healer target", spell);
}

bool RandomBotUpdateTrigger::IsActive()
{
    return RandomTrigger::IsActive() && AI_VALUE(bool, "random bot update");
}

bool NoNonBotPlayersAroundTrigger::IsActive()
{
    return !botAI->HasPlayerNearby();
    /*if (!bot->InBattleground())
        return AI_VALUE(GuidVector, "nearest non bot players").empty();

    return false;
    */
}

bool NewPlayerNearbyTrigger::IsActive() { return AI_VALUE(ObjectGuid, "new player nearby"); }

bool CollisionTrigger::IsActive() { return AI_VALUE2(bool, "collision", "self target"); }

bool ReturnToStayPositionTrigger::IsActive()
{
    PositionInfo stayPosition = AI_VALUE(PositionMap&, "position")["stay"];
    if (stayPosition.isSet())
    {
        const float distance = bot->GetDistance(stayPosition.x, stayPosition.y, stayPosition.z);
        return distance > sPlayerbotAIConfig.followDistance;
    }

    return false;
}

bool GiveItemTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "party member without item", item) && AI_VALUE2(uint32, "item count", item);
}

bool GiveFoodTrigger::IsActive()
{
    return AI_VALUE(Unit*, "party member without food") && AI_VALUE2(uint32, "item count", item);
}

bool GiveWaterTrigger::IsActive()
{
    return AI_VALUE(Unit*, "party member without water") && AI_VALUE2(uint32, "item count", item);
}

Value<Unit*>* SnareTargetTrigger::GetTargetValue() { return context->GetValue<Unit*>("snare target", spell); }

bool StayTimeTrigger::IsActive()
{
    time_t stayTime = AI_VALUE(time_t, "stay time");
    time_t now = time(nullptr);
    return delay && stayTime && now > stayTime + 2 * delay / 1000;
}

bool IsMountedTrigger::IsActive() { return AI_VALUE2(bool, "mounted", "self target"); }

bool CorpseNearTrigger::IsActive()
{
    return bot->GetCorpse() && bot->GetCorpse()->IsWithinDistInMap(bot, CORPSE_RECLAIM_RADIUS, true);
}

bool IsFallingTrigger::IsActive() { return bot->HasUnitMovementFlag(MOVEMENTFLAG_FALLING); }

bool IsFallingFarTrigger::IsActive() { return bot->HasUnitMovementFlag(MOVEMENTFLAG_FALLING_FAR); }

bool HasAreaDebuffTrigger::IsActive() { return AI_VALUE2(bool, "has area debuff", "self target"); }

Value<Unit*>* BuffOnMainTankTrigger::GetTargetValue() { return context->GetValue<Unit*>("main tank", spell); }

bool AmmoCountTrigger::IsActive()
{
    if (bot->GetUInt32Value(PLAYER_AMMO_ID) != 0)
        return ItemCountTrigger::IsActive();  // Ammo already equipped

    if (botAI->FindAmmo())
        return true;  // Found ammo in inventory but not equipped

    return ItemCountTrigger::IsActive();
}

bool NewPetTrigger::IsActive()
{
    ObjectGuid currentPetGuid = ObjectGuid::Empty;

    if (Pet* pet = bot->GetPet())
        currentPetGuid = pet->GetGUID();
    else if (Guardian* guardian = bot->GetGuardianPet())
        currentPetGuid = guardian->GetGUID();

    if (currentPetGuid != lastPetGuid)
    {
        triggered = false;
        lastPetGuid = currentPetGuid;
    }

    if (currentPetGuid != ObjectGuid::Empty && !triggered)
    {
        triggered = true;
        return true;
    }

    return false;
}

bool CraftBandageTrigger::IsActive()
{
    if (!botAI->HasSkill(SKILL_FIRST_AID) || bot->IsInCombat() || bot->isMoving())
        return false;

    uint32 spellId = CraftBandageAction::FindBestBandageSpell(bot);
    if (!spellId)
        return false;

    // Fire even at the cap if outgrown bandages are waiting to be evicted.
    if (CraftBandageAction::LowerTierBandageCount(bot, CraftBandageAction::BandageItemLevel(spellId)))
        return true;

    return CraftBandageAction::BandageCount(bot) < CRAFT_BANDAGE_TARGET_COUNT;
}

// Every engineering gadget is an item cast that would knock the bot out of
// stealth - a stealthed bot is lining up an opener (or just vanished), and
// no gadget is worth spending that on.
static bool HoldGadgetsWhileStealthed(Player* bot) { return bot->HasStealthAura(); }

bool ThrowExplosivesTrigger::IsActive()
{
    if (HoldGadgetsWhileStealthed(bot))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    Item* item = ThrowExplosivesAction::FindBestThrown(bot);
    if (!item || !ThrowExplosivesAction::CanThrowAt(bot, item, target))
        return false;

    // Always worth an explosive when it matters; only an occasional lob at ordinary mobs.
    if (target->IsPlayer())
        return true;

    Creature* creature = target->ToCreature();
    if (creature && creature->isElite())
        return true;

    if (AI_VALUE(uint8, "attacker count") >= 2)
        return true;

    return target->GetHealthPct() > 50.0f && roll_chance_i(15);
}

bool GrenadeInterruptTrigger::IsActive()
{
    if (HoldGadgetsWhileStealthed(bot))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || !target->IsNonMeleeSpellCast(true))
        return false;

    Item* item = ThrowExplosivesAction::FindBestThrown(bot, /*requireStun=*/true);
    return item && ThrowExplosivesAction::CanThrowAt(bot, item, target);
}

bool SapperChargeTrigger::IsActive()
{
    if (HoldGadgetsWhileStealthed(bot))
        return false;

    if (bot->GetHealthPct() <= 60.0f || !ThrowExplosivesAction::FindBestSapper(bot))
        return false;

    // Surrounded: several live attackers inside the blast radius.
    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    uint32 close = 0;
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && bot->GetDistance(unit) <= 8.0f)
            ++close;
    }

    return close >= 3;
}

bool TargetDummyTrigger::IsActive()
{
    if (HoldGadgetsWhileStealthed(bot))
        return false;

    if (!bot->IsInCombat() || bot->GetHealthPct() > 60.0f || AI_VALUE(uint8, "my attacker count") < 2)
        return false;

    // The dummy peels by taunting, which players and their pets ignore outright, and it is
    // never worth the tank's aggro inside an instance.
    if (!EngineeringDevices::TargetDummyWouldHelp(bot))
        return false;

    return EngineeringDevices::FindBestCarried(bot, EngineeringDevices::TargetDummies()) != nullptr;
}

bool ExplosiveSheepTrigger::IsActive()
{
    if (HoldGadgetsWhileStealthed(bot))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || target->GetHealthPct() < 50.0f)
        return false;

    if (!EngineeringDevices::FindBestCarried(bot, EngineeringDevices::ExplosiveSheep()))
        return false;

    return AI_VALUE(uint8, "attacker count") >= 2 || roll_chance_i(10);
}

bool JumperCablesTrigger::IsActive()
{
    if (bot->IsInCombat())
        return false;

    // Classes with a real resurrection spell don't need to improvise.
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
        case CLASS_PALADIN:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
            return false;
        default:
            break;
    }

    if (!EngineeringDevices::FindBestCarried(bot, EngineeringDevices::JumperCables()))
        return false;

    return AI_VALUE(Unit*, "party member to resurrect") != nullptr;
}

bool RocketBootsTrigger::IsActive()
{
    if (HoldGadgetsWhileStealthed(bot))
        return false;

    if (bot->IsMounted() || bot->HasUnitState(UNIT_STATE_ROOT))
        return false;

    if (!EngineeringTinkers::UsableEquipped(bot, EQUIPMENT_SLOT_FEET, /*requireSpeedBurst=*/true))
        return false;

    // Carrying a battleground flag: hit the boosters immediately.
    if (bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) || bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG))
        return true;

    // Chasing the enemy flag carrier who is pulling away.
    if (Unit* target = AI_VALUE(Unit*, "current target"))
        if (target->IsPlayer() &&
            (target->HasAura(BG_WS_SPELL_WARSONG_FLAG) || target->HasAura(BG_WS_SPELL_SILVERWING_FLAG)) &&
            bot->GetDistance(target) > 10.0f)
            return true;

    // Last-ditch escape.
    return bot->GetHealthPct() < 25.0f && AI_VALUE(uint8, "my attacker count") > 0;
}

bool GloveTinkerTrigger::IsActive()
{
    if (HoldGadgetsWhileStealthed(bot))
        return false;

    if (!bot->IsInCombat())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    return EngineeringTinkers::UsableEquipped(bot, EQUIPMENT_SLOT_HANDS, false) != nullptr;
}
