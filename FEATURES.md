# Felworld changes in detail

The full tour of what [Felworld](https://github.com/felworld/azerothcore)'s
fork of mod-playerbots adds and changes, with the config options behind each
behavior. The [README](README.md) has the summary; upstream behavior,
commands, and strategies are documented in the
[mod-playerbots wiki](https://github.com/mod-playerbots/mod-playerbots/wiki).

## Command prefix

Bot commands require a `!` prefix. Felworld sets upstream's
`AiPlayerbot.CommandPrefix` option to `!`, so every command from the
[playerbot command list](https://github.com/mod-playerbots/mod-playerbots/wiki/Playerbot-Commands)
is written `!follow`, `!attack`, `!who warrior`, etc. Messages without the
prefix are never parsed as commands — they're ordinary chat, which (in LLM
mode) goes to mod-llm instead of being silently eaten
because it happened to start with a command word ("who said that?"). Our
fork also fixes the bot's internally re-queued commands (repeated `cast`)
to respect the prefix, which upstream's option didn't.

## Quest-aware grinding

The `grind quests` strategy: like upstream's `grind` ("attack any visible
target"), but the bot only engages mobs that someone in its group still
needs for an incomplete quest — kill credit or a quest-item drop —
including neutral and gray ones. The result is a bot that pulls and fights
like a questing partner instead of slaughtering everything in sight.
Toggled with the `!grind quests` chat command
([below](#commands-added-in-this-fork)).

## Runner focus fire

When a dungeon mob breaks off at low health to fetch reinforcements, DPS
bots now switch to it and burn it down before the adds arrive, and
snare-capable classes (Concussive Shot, Hammer of Justice, Hamstring)
aim their slows at it. Previously such runners were invisible to bot
targeting: the "flee for assistance" behavior looks nothing like a
feared mob under the hood, and the runner's sprint out of attack range
actively deprioritized it. A skull raid mark still outranks the runner —
a deliberate kill order is never overridden — and tanks ignore it and
keep holding the pack. Fear-based crowd control is unaffected (feared
mobs move differently from runners at the engine level), and the
behavior only applies inside instances, so open-world fleeing mobs are
simply allowed to leave.

## Human reaction latency

Reactive triggers used to fire on the first AI tick after their cause
appeared: an enemy cast got kicked the instant the cast bar lit up, a
root got Hand of Freedom before the victim had visibly stopped, every
time, with metronomic consistency — the single most reliable tell that
nobody was at the keyboard. Now the triggers that model human twitch
reactions — interrupts, dispels/decurses (including Hand of Freedom on
a rooted ally), and emergency heals — wait out a jittered per-event
reaction delay before acting. Each event rolls its own delay, so kicks
land at varying points into a cast (occasionally panicky-instant, the
way real players pre-aim interrupts), and a second rooted ally never
inherits the delay already served for the first.

Bots also sometimes miss a reaction outright, like a human who didn't
catch the cast: a small per-category miss chance makes the bot wait out
the whole reaction window and only then re-assess, rolling again if the
situation still holds. Short enemy casts simply get away; a still-rooted
or critically-hurt ally is eventually noticed late rather than never.
Movement, rotation, and positioning triggers are untouched and stay at
full responsiveness. Per-category `AiPlayerbot.ReactionDelay*Min`/`Max`
windows (defaults 200–1500 ms interrupts, 300–2000 ms emergency heals,
400–2500 ms dispels) and `AiPlayerbot.ReactionMissChance*` percentages
(default 5) in `playerbots.conf.dist`; set a `Max` to 0 to restore the
robotic instant reactions for that category.

## Pet group etiquette

Hunter, warlock, death knight, frost mage, and enhancement shaman pets
behave like a considerate group member's pet in dungeons. They assist
the owner's current target instead of free-engaging whatever the core
pet AI fancies, never initiate combat on an unpulled mob in instanced
group content, and get called back to heel when their owner disengages
rather than dragging their aggro into the next pack. Taunt autocasts —
Growl, Torment, Suffering, Anguish, all ranks — are switched off while
grouped in an instance (and back on when solo, where a pet tanking for
its owner is the point), and a pet configured aggressive via
`AiPlayerbot.DefaultPetStance` is clamped to defensive while grouped.

The same taunt autocasts also come off against players. A taunt does
nothing to a player — players have no threat list — but the pet still
spends a global cooldown on it every time it comes off cooldown, which is
a global cooldown not spent on Bite, Claw, or a Kill Command follow-up. A
battleground or arena counts as PvP for the whole match; out in the world
it is whether a player, or a player's pet, is the bot's target, on the
bot's attacker list, or what the pet itself is swinging at. Taunts stay
off for ten seconds after the last player opponent, so a fight that
alternates between players and their pets or nearby mobs does not flap the
pet bar. Upkeep runs in combat as well as out of it, since who the pet is
fighting is only known once the fight is on.

## Dungeon pulls by the tank

Who opens on the next pack in a dungeon used to be whichever bot happened
to hold group leadership: the Dungeon Finder picks a leader at random
unless someone ticked "Leader", a bot leader gets the open-world `grind`
kit, and inside an instance that kit has no leash and no aggro-range
check — so a hunter leader would shoot the nearest mob in line of sight
while the real tank was still drinking, and every other DPS would assist
the moment it had threat. Upstream's "I don't know this dungeon, lead the
way!" hand-off didn't help: it re-derived the bot's strategies before the
leader change had actually landed, so the bot kept the kit.

Initiation in instanced group content is now a role, not a leadership
accident. With `AiPlayerbot.DungeonPullByTank` (default 1) the opener is
the group's main tank — the main-tank flag if one is set, otherwise the
tank by role (Dungeon Finder badge for humans, spec for bots). When that's
a real player, no bot starts a fight, period; DPS bots still assist
anything that has threat. When the main tank is a bot, it pulls the
nearest unengaged pack in its line of sight — with its ranged pull
ability (Shoot, Icy Touch, Judgement, Faerie Fire) through the existing
pull machinery, so pets get parked and it returns to its pull spot, or by
walking in when it has no such ability or weapon — but only once the whole
group is ready: everyone on the map, alive, out of combat, within
`AiPlayerbot.DungeonPullGroupRange` yards of the tank (default 30), not
sitting to eat or drink, and above `AiPlayerbot.DungeonPullMinHealth` /
`AiPlayerbot.DungeonPullMinMana` percent (defaults 80 / 60). That gate is
the ready check done the way a human at the keyboard reads it — standing
back holds the tank, walking up releases it — with no popups; `!stay` and
`passive` remain the hard stop. A ranged opener is taken from the pull
spell's own range rather than walked in from across the room: the tank
opens from where the ready check found it — with the group, since everyone
is inside `DungeonPullGroupRange` of it — and returns to that spot once the
pack is on it, so the fight is dragged back to the party instead of being
had where the pack stood, next to its neighbours. A pack further off than
that is not opened on at all while the tank has someone to follow, since
the party's pace is the master's; leading an all-bot group there is nobody
to wait for, so the tank walks up to its own pull range first. In all-bot
groups the leader hands
leadership to the tank ("Thrallok, you lead.") since bots follow their
leader, and the ex-leader now sheds its grind kit only after the leader
change has landed. Pulls are logged as `dungeon_pull` events. Without any
tank in the group the leader keeps grinding as before; `0` restores the
upstream behaviour everywhere.

## Dungeon follow spread

Following used to park every bot 1.5 yards from whoever it follows, no
matter what it does in a fight. In a dungeon that meant the healer and
every ranged DPS — and their hunter and warlock pets, which trail their
owner at a couple of yards — walked into the pack with the tank, so
approaching one group of mobs proximity-aggroed the ones beside it.

Inside instanced group content the follow slot is now role-aware in
distance, not just in angle. Ranged DPS hold their own attack range
behind whoever they follow (spell range less a few yards, so about 25
with the default `AiPlayerbot.SpellDistance`): mobs get pulled to the
tank, so a bot standing at attack range never needs to step forward.
Healers hang back as far as their heals still cover the group — heal
range less a buffer, checked against where the other members actually
are, so nobody drops out of reach. Tanks and melee DPS keep the tight
follow slot, but every slot — tight or spread — is folded into a
120°–240° fan behind the leader: the stock formation ring runs all the
way around to the leader's flanks (and, in a two-man, straight past his
nose), which walks a bot point next to the puller and body-pulls the
next pack. Relative slot order is preserved, so bots still fan out from
each other instead of stacking.

"Behind" is tracked with hysteresis rather than read off the leader's
live facing: while he is covering ground it is derived from his path
(resampled every few yards), so mouse-look mid-run doesn't send the
group orbiting him; standing still, the fan only re-forms once he has
turned more than 60°.

Standing back only helps if the back is empty, so a spot is taken only
once it clears a safety check: line of sight and a walkable path to the
leader, and no living, non-critter mob that isn't already in the fight
within its own aggro radius (plus a margin) of it. A spot that fails
collapses toward the leader in steps until one passes, and if none does
the bot keeps the tight follow slot — itself in the rear arc, so the
degraded case still stays off the leader's toes. The slot is re-resolved every
few seconds or once the leader has actually travelled, so it doesn't
whip around every time he turns. Outside dungeons and raids, and for
bots that aren't grouped, following is unchanged. There are no config
knobs: the buffers are internal.

## Hold fire until the tank has it

Spreading the group out stops the approach from body-pulling the room, but
it does nothing about the pull itself. The instant a tank's opener landed,
every DPS bot charged, opened fire and sent its pet in — on a mob that was
still running across the room with no threat on anything. Whoever's spell
landed first held it, the mob turned around mid-run, and a melee bot that
had sprinted past the tank to reach it dragged the pack beside it in too.

In instanced group content every bot that is not the main tank now
withholds offense against a mob until that mob is *tank-engaged*
(`AiPlayerbot.DungeonHoldForTank`, default 1). Held means held: no
attacks, no offensive casts, no gap-closers, no walking into melee or
spell range, and no pet sent in. It is decided per mob, not per pull, so
the melee half of a pack releases on one clock and a stray caster on
another. A mob counts as tank-engaged when any of these is true:

- It has stood inside the main tank's melee range for
  `AiPlayerbot.DungeonHoldEngageDelay` milliseconds (default 1500) and is
  attacking the tank. This is the ordinary case — the tank opens, the pack
  runs to it, and the tank gets its Sunder or its Consecration down before
  anyone else touches anything.
- It is attacking any other group member — a healer, a ranged bot, the bot
  itself, or somebody's pet. Whatever the tank meant to happen has already
  failed; holding would only give the mob free time on somebody who cannot
  take it.
- `AiPlayerbot.DungeonHoldTimeout` milliseconds (default 5000) have passed
  since the group entered combat with it. Casters and ranged mobs shoot
  the tank from where they stand and never walk into its melee, so melee
  time alone would hold the group off them for the whole fight.

What is never held: healing, buffing, cures, following, formation
movement, and defending itself — a mob that turns on the bot is engaged by
the second condition, so self-defence is automatic. The main tank is never
held, and with no main tank in the group (nobody flagged, nobody tanking
by role) nothing is held at all, so an ungrouped or tankless bot behaves
exactly as before. Enemy players are never held either: a player has no
threat table for a tank to build on.

Bots keep taking and facing their target through the hold — only the swing
is kept back — so the fight starts on the tick the hold releases instead of
a target acquisition later. Since the class strategies dispatch the pet and
start the swing from an edge-triggered "target changed" that has long since
fired by then, both the pet and the bot's own attack are re-dispatched off
level-based release triggers that re-check twice a second.

Pets are on the same leash. Hunter and warlock pets, death knight ghouls,
the frost mage's water elemental and the enhancement shaman's wolves are
all dispatched through one shared assist action, so the gate is one gate:
the pet is not sent until the mob is tank-engaged, and it *is* sent the
moment it becomes so. Stance is untouched — a defensive pet whose owner is
holding fire has nothing to react to, and taunt autocasts are already off
in instances (see [Pet group etiquette](#pet-group-etiquette)).

Setting `AiPlayerbot.DungeonHoldForTank` to 0 restores the upstream
behaviour, where everyone opens the instant the pull lands.

## Attack what the tank is attacking

Holding fire settles who opens; it says nothing about what to open on.
Once the pull landed, each DPS bot scored the pack for itself — nearest,
lowest health, longest expected life — so five bots routinely came up
with five different answers, and the adds they picked were the ones
already walking to the tank. The tank spent the fight chasing threat on
mobs that were being killed behind its back.

In instanced group content DPS bots now assist the group's tank:
whatever the tank is swinging at is the group's target, and the bots
move with it the moment the tank switches. The tank is whoever carries
the main-tank raid flag, or failing that the first alive member tanking
by role — a human counts, read from the Dungeon Finder role badge and
falling back to spec. Tanks and off-tanks are unaffected; they keep
picking up what nobody else has.

The preference yields in the cases where it should. A skull mark still
outranks it — a deliberate kill order is never overridden. A mob fleeing
for reinforcements still outranks it, since the adds it is fetching cost
more than the tank's current target (see
[Runner focus fire](#runner-focus-fire)). And when the tank has nothing
usable — between pulls, dead, out of the room, or its target crowd
controlled or untouchable — the bot scores the pack for itself exactly
as before. Open-world and PvP targeting is unchanged: there is no pull
order to hold to out there, and funnelling every bot onto one mob would
read as a single-minded blob.

Damage dealers also stop picking crowd-controlled mobs. Sheeped, sapped,
shackled and hibernated targets were already dropped from the attacker
list, but the list is rebuilt once a second and lets raid-marked mobs
through unfiltered, so a mob polymorphed a moment ago stayed on the menu
just long enough for a row of DPS to break it and set off the re-sheep
loop. The check is now re-run live at target selection. Roots stay
excluded on purpose — a rooted mob is what kiting classes want to keep
shooting — and if a controlled mob is the last hostile standing,
breaking it beats waiting the control out.

## Threat discipline in unscripted dungeons

Upstream ships a per-map table of instance strategies, and outside the
raids it stops at Wrath: every classic and Burning Crusade five-man —
all of Maraudon, Scholomance, Sunken Temple, every Auchindoun wing —
fell through it and ran the bots' open-world kit. That kit has no
notion of a threat table, so a mage that had been told to attack the
tank's target simply cast until the mob turned around, and the first
pull of a Maraudon run had three mobs loose on the healer.

Any non-raid dungeon map with no bespoke script now gets a generic kit
instead of nothing:

- **Threat discipline.** Every bot that is not the tank stops offense
  against a mob once its own threat reaches 90% of the main tank's on
  that mob (60% for anything that hits the whole pack, since one AoE
  takes every mob at once). Melee bots keep auto-attacking through the
  throttle and everyone keeps their target — only abilities and casts
  stop, the way a player watching the threat meter stops, and they
  resume as the tank's lead grows back. Heals, shields and buffs are
  never throttled. Mobs the tank has not touched at all are not this
  rule's business: whether anyone may open on those is decided by
  [Hold fire until the tank has it](#hold-fire-until-the-tank-has-it).
- **Automatic AoE dodging.** Upstream hands out AoE dodging only to
  bots whose master is a real player, which leaves a bot-only party
  standing in the fire. In a dungeon every bot dodges, master or no
  master (`AiPlayerbot.AutoAvoidAoe` still turns the whole thing off).
  Two gaps in what "the fire" means are closed with it. Upstream only
  notices a ground hazard once its debuff has already landed on the
  bot, so a cloud the bot is immune to, or one it has only just walked
  into, is invisible; bots now look for the hazard itself — any hostile
  ground effect within `AiPlayerbot.MaxAoeAvoidRadius` that deals
  damage, directly or through what it periodically triggers — and step
  clear of it with a few yards of slack rather than stopping on its
  edge. And dodging now runs out of combat as well as in it: a cloud
  outlives the caster that dropped it, so it is at its most lethal
  exactly when the pull is over and the party is standing in it
  looting and drinking. This is generic — no spell or map is named
  anywhere in it — but the case that prompted it was Maraudon's
  Noxious Cloud, 150 damage a second in a five-yard puddle centred on
  a slime the melee were hitting.

Maps with a bespoke pack are left alone — the raid and Wrath five-man
scripts manage priority through their own multipliers. Everything else
carries over unchanged: the tank still makes the pull
([Dungeon pulls by the tank](#dungeon-pulls-by-the-tank)) and nobody
opens before it lands.

## Crowd control that knows when to stop

A mage watching its Polymorph break would simply cast it again, and
again, on a mob the tank had already picked up and the group was
already burning. The recast trigger refires the instant the control
drops, and nothing in it ever asked whether the mob was still worth
controlling — so a sheep that broke to a stray cleave came straight
back, broke again on the next tick, and the mage spent the whole fight
re-sheeping a mob at 40% health instead of doing damage.

Crowd control is now offered only against a mob the group has not
committed to killing. A candidate is refused when it has dropped below
90% health — "untouched", not merely "healthy": anything the group has
already chipped should just die — or when any group member or their pet
is swinging at it, which covers the tank's current victim and a human
tank equally. It deliberately does not ask who the *mob* is attacking:
on a pull the whole pack runs at the tank, and those adds are exactly
what the control is for.

On top of that each bot carries a per-fight recast budget of three
applications per mob. A control that breaks to bad luck comes back once
or twice; past that the mob has proved it will not stay controlled and
the bot moves on. The ledger is keyed by GUID, wiped when the bot
leaves combat, and ages out on its own so chain pulls that never drop
combat still reset.

Enemy players answer to the budget alone — diminishing returns already
price their re-controls, and the solo breather Polymorph wants an
opponent that is losing. A raid icon set for crowd control buys no
exemption: a marked mob the group has started killing is still not
re-sheeped.

## Off-role bots do not main-heal

A boomkin spent a dungeon fight throwing Rejuvenations at the tank.
Every druid, shaman and paladin carries party-heal actions regardless
of spec, and because a bot's non-combat engine keeps running while it
is in combat — it flips back the moment its current mob dies — those
heals fire mid-fight. A ret or prot paladin would likewise Lay on Hands
a party member straight from its combat rotation. Nothing in the
trigger, the target selection or the heal action ever asked whether the
bot was a healer.

A bot whose spec is not a healing spec now withholds heals on other
party members while in combat unless it is a genuine emergency: the
target is below 35% health *and* no healer-spec group member is alive,
on the map, within heal range of them and holding at least 15% mana.
Anything short of that loses to the bot's own rotation, so a boomkin
nukes and a ret paladin swings. Group heals get no exception at all —
there is no one dying member to make it for.

Healer specs are untouched, and so is everything out of combat: topping
the party up between pulls is exactly what an off-spec healer is good
for. A bot healing itself is untouched too — that is survival, not
main-healing. Human healers count as cover through their Dungeon Finder
role, so a DPS bot in a party with a human healer stays on damage.

## Dependable LFD port-in

Bots that queue through the Dungeon Finder now reliably arrive in the
dungeon. The core teleports a freshly formed group exactly once and
silently skips anyone who happens to be falling, fighting, dead, or on a
vehicle at that instant — common states for a wandering bot — so groups
could form with a member stranded outside. Stranded bots now notice and
retry every few seconds until they land. A bot that's mid-fight when the
group-ready proposal pops also no longer declines it (which dissolved
the proposal for everyone); it defers and accepts once the fight ends.

## Warsong Gulch teamwork

Warsong Gulch bots play the objective like a team. Upstream's
"protect the flag carrier" behavior never actually ran (the trigger was
never registered); on top of fixing that, our fork adds dedicated escorts
that stick with the flag carrier, peels where anyone near the carrier
attacks enemies closing in on it (before the first hit lands, not after),
home defenders that hold the flag room before the flag is ever picked up,
and roles that are re-decided on death — like a real player choosing what
to do from the graveyard, biased toward whatever is happening nearby.
Sneaking matters too: rogues Stealth and druids of any spec Prowl when
approaching the enemy flag room or nearby enemies (and no longer drop
stealth with enemy players around), while Night Elves without native
stealth Shadowmeld when guarding the flag room or waiting with a
stationary carrier. Bots also communicate like teammates: whoever sees
enemies closing on the friendly flag room calls it in battleground chat
("3 incoming at our flag room!") — one callout per team per wave, not ten
bots shouting at once.

Communication works the other way too: you can call plays. Saying
`!bg strategy attack fc`, `!bg strategy attack base`, `!bg strategy
defend fc`, or `!bg strategy defend base` in battleground chat orders your
bot teammates to chase the enemy flag carrier, push the enemy flag room,
escort your flag carrier, or fall back and hold your own flag room. Like
real teammates, not everyone listens: each bot independently follows the
call with `AiPlayerbot.BgStrategyComplianceChance` percent probability
(default 65), announcing in battleground chat what it's doing ("Going
after their flag carrier!") so you can see exactly who is responding. The
order overrides a bot's normal role for
`AiPlayerbot.BgStrategyOrderDuration` seconds (default 45), then it
drifts back to whatever it was doing — and repeating the call re-rolls
the bots not yet on the play, so calling again rallies more of the team
(bots already complying just quietly keep at it, on a refreshed
timer). Flag carriers ignore orders and keep running the flag,
and `fc` calls are ignored when the flag carrier in question doesn't
exist. The team's overall temperament (offense-heavy vs. defense-heavy,
rolled at match start) stays what it is — no amount of chat whipping
changes a team's nature, only its next move.

This order machinery is also the hook behind mod-llm's `bg_strategy`
tool: in LLM mode, one bot reads a natural callout like "inc!!" or "fc
mid" in battleground chat and relays it as a play call for the whole
team — every bot (the interpreter included) rolls the same compliance
dice and announces the same way as if the command had been typed. See
[mod-llm's battleground play calls](https://github.com/felworld/mod-llm/blob/main/FEATURES.md#battleground-play-calls).
Setting `AiPlayerbot.BgStrategyComplianceChance = 0` disables play calls
entirely, the LLM path included.

## Staggered battleground mounts

Waiting behind the gates, bots mount up one by one rather than all on the
same tick. Upstream gates battleground mounting on a single "less than 30
seconds to go" check, so an entire team snaps onto its mounts in unison the
moment the timer crosses it — the tell that they aren't people. Each bot now
picks its own moment in the last 30 seconds of the prep phase (down to five
seconds before the gates open), derived from who it is and which match this
is, so the starting area fills with mounts gradually and a bot doesn't mount
at the same instant twice in a row.

## Bystander assist

Solo random bots rescue nearby non-group players — real
players and bots alike — who look like they're about to die. "About to
die" is more than a health threshold: rapid health loss, being swarmed,
or a drained mana pool all count (a caster out of mana at half health is
doomed long before a raw HP check would fire), and healer-class victims
that still have mana are trusted to save themselves a while longer.
Priests, paladins, druids (dropping form if needed), and shamans heal the
victim; other classes charge the attacking mob. Bots won't dogpile a
fight that already has helpers, won't join one they judge unwinnable —
a composition estimate where an elite counts as roughly three normal
mobs (so two players take on one elite, but not two, and world bosses
and mobs far above the bot's level are never taken on) — and never
take an assistive action
that would newly PvP-flag them. `AiPlayerbot.EnableBystanderAssist`
(default on) plus threshold knobs in `playerbots.conf.dist`.

Players losing a PvP fight qualify too — the support half of [passerby
assist](#passerby-assist), and the only help a bot too far below the
enemy to fight can offer. Healer classes support such victims with
heals, never by attacking (joining the fight is passerby assist's job),
each supporter first passing `AiPlayerbot.BystanderPvpSupportChance`
(default 50%, deterministic per bot/victim pair), and nobody heals into
a dogpile of more than two player attackers. The first rescue heal pulls
the healer into combat; it stays on its victim through that combat until
the victim is safe or out of range, or the healer's own
self-preservation gates trip (the enemy turning on the healer ends the
rescue the moment the beating shows). The flagging rule is unchanged: an
unflagged bot never heals a flagged victim.

## Social buffing

Idle ungrouped mages, priests, druids, and paladins cast
their signature class buff (Arcane Intellect, Fortitude, Mark of the Wild,
a fitting Blessing) on nearby friendly players — real players and bots —
who lack it, and buff-capable bots return the favor when someone buffs
them. Because blessings from different paladins stack, a paladin doesn't
give up on someone who already has one: it works down the same
role-based priority list the greater-blessing assignment uses (so a
target wearing Might gets Kings) and offers the best flavour they aren't
already carrying, never Might to a mage, priest, or warlock and never
Wisdom to a warrior, rogue, or death knight. Generosity is paced: a bot
gives at most one walk-up buff per `AiPlayerbot.SocialBuffGiverCooldown`
window (default 60s), so in a busy spot it blesses whoever happens to be
nearest rather than methodically working through the crowd — buffing
back is exempt. A bot that is eating or drinking finishes its meal
before volunteering. Bots also answer a stranger's heal with a targeted /thank emote
(mod-llm supplies the spoken "thx"). Like bystander assist, no action is
ever taken that would newly PvP-flag the bot: flagged targets are only
buffed by already-flagged bots. `AiPlayerbot.EnableSocialBuffing` and
`AiPlayerbot.EnableHealThanks` (both default on) plus radius/cooldown
knobs in `playerbots.conf.dist`.

## Stealth-spotting reactions

Upstream bots walked straight past a rogue they could technically see
and never acknowledged them. Now, when a bot's stealth detection
genuinely reaches a stealthed player nearby — the same math the server
uses for real players (level plus stealth/detect auras, up to 30 yards),
except in all directions, since the client's stealth-detect ping isn't
directional — it has the "oh crap" moment a real player has: it freezes
mid-stride, snaps around to face the stealther, and hesitates a beat
before carrying on. Friend or enemy, it's startling either way.

Sometimes the fright resolves into an emote a moment later: a /wave at a
friendly sneak, but only when no enemy is around who could use the wave
to find them; or a /point calling out a hostile one, but only when a
friendly is nearby to warn and the bot isn't PvP-flagged (a flagged bot
in a contested situation fights rather than points). Group members never
trigger the reaction — stealthed party members are always visible to
their own group — and a bot that is itself stealthed stays quiet rather
than give its position away. `AiPlayerbot.EnableStealthReactions`
(default on) plus cooldown/emote-chance knobs in `playerbots.conf.dist`.

## Stealth flushing

Startling is one thing; doing something about it is another. Each bot
keeps a short perception ledger of the enemies around it, and when one
it could perceive disappears *while hidden* — a rogue Vanishing
mid-fight, a duel opponent stealthing during the countdown, a spotted
sneak slipping back out of detection range — the bot forms a suspicion
anchored to the last place it perceived them. Someone who simply ran
away in the open is forgotten, not hunted.

For the length of the suspicion window the bot sweeps that spot the way
a player would, and classes with a flush tool use it: paladins drop
Consecration on arrival, hunters Flare the spot from range and mine it
with a trap when out of combat, mages and priests pulse Arcane
Explosion/Holy Nova, death knights put Death and Decay on it, shamans
plant a Magma Totem. Classes without a tool still walk over and search.
Flushing never replaces fighting: the moment the stealther is directly
perceivable again the suspicion clears and ordinary targeting takes
over — which for druids means tagging a revealed rogue or fellow druid
with Faerie Fire ahead of the normal rotation, locking out
Vanish/restealth for its duration.

Suspicions only form against enemies the bot would fight anyway — the
same-class truce and corpse-camping satiation boards are honored, and
unflagged players are never hunted — except a duel opponent, who always
counts. The flush roll happens once per disappearance
(`AiPlayerbot.StealthFlushChance`, default 70), so some bots shrug and
move on while most give chase for `AiPlayerbot.StealthFlushSeconds`
(default 15). Under the `AiPlayerbot.EnableStealthReactions` umbrella;
`0` chance disables flushing without touching the startle reactions.

## Quest-competition groups

A solo random bot that sees a nearby ungrouped
same-faction real player — within 4 levels — fighting a
creature the bot still needs for an in-progress quest silently invites
them to a group, the way real players resolve spawn competition. While
grouped the bot behaves like a questing partner: it sticks with its new
partner between fights and peels off to attack whatever mobs anyone in
the group still needs, so the pair shares kill credit and moves to the
next camp together. Mob types the bot fights that a member still needs
extend the group's shared objectives — but unrelated shared quests two
zones away never hold it together. Once nobody in the group needs any of those mobs, the bot
says thanks in party chat and leaves. Declined invites go on a
per-player cooldown so nobody gets pestered.

The group keeps growing the same way once you are in it: the bot
recruits other bots it catches competing for the same spawns, up to a
full party. Recruits are free-roaming random bots only — never someone
else's altbot — and they run the episode themselves rather than tagging
along, so they grind the shared objectives as peers and say their own
goodbyes when the camp is done instead of staying glued to you after
the bot that invited them leaves. Bots only recruit into a group you
are already in: a bot-only grinding party has nobody to see it, and the
random bot manager takes it apart anyway.
`AiPlayerbot.QuestCompetitionInvite` (default on),
`AiPlayerbot.QuestCompetitionInviteCooldown`, and
`AiPlayerbot.QuestCompetitionGroupSize` (default 5, party sizes only —
set it to 2 to keep the group to the bot and you) in
`playerbots.conf.dist`.

## Chest roll-offs

In a group that contains a real player, bots don't ninja-loot world
chests anymore: when the group spots a lootable chest, every bot that
could open it does a visible `/roll` (1–100) and only the highest
roller walks over and opens it. The real player can enter the contest
too by typing `/roll` within the ~5-second window — and if they win,
the bots leave the chest alone (bots re-consider it after 60 seconds if
the winner never collects, so a passed-up chest doesn't stay locked
forever). Only genuinely contested chests get a roll-off: gathering
nodes, quest chests, chests with group loot rules (whose contents
already go through real need/greed rolls), and chests that everyone can
loot anyway — the ones that aren't consumed by looting or that restock
on a timer, so the next player who opens them gets their own copy (the
Scarlet Monastery fireworks rockets are the classic case) — are exempt,
as are all-bot groups, which loot the way they always did.
`AiPlayerbot.ChestRollEnable` in `playerbots.conf.dist` (default off
upstream-style; enabled in the Felworld config tree).

## Roll-win giveaways

A bot that wins a group loot roll on a weapon or armor piece it has no
use for — vendor trash to it, but a genuine upgrade for someone else in
the roll — sometimes walks over and hands it to them. The judgment uses
the same class/spec/current-gear scoring bots use for their own votes,
applied to every roll participant including members who passed (polite
players pass on things they could use). The real player is the
preferred recipient: the bot runs up, opens a trade with the item in
it, and says why; bot recipients accept silently. Soulbound roll wins
respect the 2-hour BoP trade window, so only members who were eligible
for the original loot can receive them. Only active in groups
containing a real player. `AiPlayerbot.RollWinGiveawayChance` in
`playerbots.conf.dist` (0–1 probability per eligible win; default 0,
set to 0.6 in the Felworld config tree).

## City market trading

Bots buy and sell with players for real, through actual trade windows.
Whisper a bot `!wts <itemlink> [count] [price]` and it answers as a buyer:
if the item is genuinely useful to it — an equipment upgrade, a reagent it
ran out of, a consumable it is low on — it quotes a price or, when you
named a sane one, commits on the spot, comes over, and pays you through a
trade window. `!wtb <itemlink> [count] [price]` is the mirror: the bot
sells from its tradeable spare stock (the things it was otherwise going to
vendor). `!appraise <itemlink>` asks what an item is to that bot and what
it would pay or ask; `!sellables` lists its stock and wants with quotes;
`!sellto`/`!buyfrom <player> <itemlink> [count] <price>` are the explicit
commitment commands (master-only for players; mod-llm's `commit_trade`
tool uses them after a chat negotiation). Deals only ever commit against a
real player — bots posting ads at each other stays chat ambience, no goods
move.

Everything mechanical is deterministic: prices come from
mod-ah-bot-plus's valuation when that module is enabled (already jittered
per call, so quotes never exactly match AH listings), falling back to a
vendor-price heuristic; a bot never sells below half its own quote or
vendor price, never pays more than double its quote, and caps purchases
by the same free-gold budget it reserves for repairs and training.
Quotes (service tips included) round down to whole silver — player trades
deal in silver and gold, never loose copper — and an item that cannot
fetch at least one silver has no market price at all: it gets vendored,
not advertised or bought. Anything an NPC vendor sells for plain gold in
unlimited stock (reagents like Symbol of Kings) has no player market
either — never advertised, never wanted — while limited-stock vendor
recipes and honor/token items keep theirs. Ad stock also skips leveling
leftovers: an
uncommon-or-worse item more than 20 levels below the bot stays out of its
WTS lists (rare and better gear is hawked at any level), though a player
who explicitly asks to buy such an item still gets a deal. A
committed deal is fulfilled like the roll-win giveaway: the bot walks up,
opens the trade, places the agreed stacks or gold, and only accepts while
your side of the window actually covers the deal — a short-changed offer
just times out. Sell counts round up to whole stacks, with the overshoot
thrown in.

A counterparty beyond walking range gets met the way the
[world-PvP excursions](#world-pvp-excursions) travel: the deal holds for a
simulated ride (distance-paced on the same continent, a flat ~4 minutes
for a boat/zeppelin hop; the confirmation whisper names the bot's current
zone and asks for a few minutes), then the bot arrives a couple hundred
yards out — never where a real player could watch the blink — and walks
in to trade. Local deals expire in 2 minutes, traveled ones get the ride
plus 5; a bot holding a deal also stops rolling flight paths or cross-zone
treks until it's done.

Posting or engaging with market chatter stamps a short anchor
(`AiPlayerbot.TradeAdAnchorSeconds`, default 2 minutes, renewed on
engagement) that makes the [busy-capitals](#busy-capital-cities) dwell
guaranteed instead of 80%, so an advertising bot doesn't port out while a
buyer is typing; a committed deal extends it
(`AiPlayerbot.TradeDealAnchor{Min,Max}Seconds`, default 5–10 minutes).
`AiPlayerbot.KeywordTradeReplies` gates upstream's keyword-matched "WTB"
chat responder, which should be off in LLM sessions where
[mod-llm](https://github.com/felworld/mod-llm) drives ad reading, Trade
chatter, and negotiation on top of these commands.

## Busy capital cities

Bots that find themselves in a friendly (own-faction or neutral) capital
mostly stay a while, wandering between the bank, auction house, inn, and
vendor NPCs or sitting down to rest, instead of immediately rolling their
next activity somewhere out in the world. Combined with upstream's
periodic teleport-to-a-city-banker (`AiPlayerbot.ProbTeleToBankers`),
this keeps a standing crowd around the Orgrimmar and Stormwind bank/AH/inn
districts, the way capitals feel on a busy server — while bots still
eventually leave, so the faces turn over. `AiPlayerbot.CityDwellChance`
(default 0.9) is the per-reroll probability of staying; bots re-roll about
every 5 minutes, so the mean visit is `5min / (1 - chance)` (~50 minutes
at the default). Set to 0 to disable.

Gate duelists feed the crowd too: when a bot's duel-spot hangout outside
the Stormwind or Orgrimmar gates runs its course
(`AiPlayerbot.DuelSpotDwellMinutes{Min,Max}`, default 10–25 minutes), it
walks in through the gates to the bank district and potters there like
any other city dweller, instead of idling outside the walls — where the
zone isn't a capital, so the next activity roll would immediately send it
somewhere else in the world.

## No city picnics

Bots don't sit down to eat or drink inside capital cities. The low-health
and low-mana reflexes that make a bot break out food and water in the
field used to fire anywhere out of combat, so a bot hearthing home after
a grinding session would plop down cross-legged in the middle of the
Orgrimmar or Stormwind bank district for a meal — one of the more
recognizable "nobody's home" tells, since real players just don't do
that. Inside a city rest area (the same zones that accrue rested XP) the
eat/drink actions are now suppressed entirely: low bars wait for natural
out-of-combat regeneration, or for the bot to walk back out of the
gates, where the normal behavior resumes immediately. A meal already in
progress when the bot crosses the city border still finishes. Inns and
taverns outside capitals are unaffected. No config option — it's always
on.

## Sitting means eating or drinking

Bots have no reason to sit unless they are having a meal. Upstream mirrored
the party leader: the moment you sat down, every bot within 10 yards
dropped to the floor on the same server tick and stood back up with you —
a synchronized tell no group of people produces. That mirroring is gone.

The leader resting is still read as a signal, just a different one: it's
the group stopping, so it's the moment to top off. A bot within 20 yards
of a resting master, out of combat and below 95% mana (or health), breaks
out food or water even though its usual low-resource bar hasn't been hit —
each bot after its own deterministic delay of up to 4 seconds, so the
answers arrive spread out rather than in unison. Everything else that puts
a bot on the ground (its own meal, the `sit` command, idle roleplay sits)
is unchanged. No config option.

## Meals finish before the loot run

A bot that sits down out of combat drinks or eats to full before it picks
anything up. Looting outranked the "keep sitting" hold, and every movement
action stands a bot up, so a corpse or a herb noticed mid-drink yanked the
bot off its water; it re-drank, got yanked again, and a dungeon healer
could ride that loop from empty to empty. The hold now outranks the whole
loot chain and releases itself the moment the bot is full or enters
combat, after which the loot the bot was queuing up happens normally.
Chat commands and combat reactions still cut a meal short. This also
unblocks tank-led dungeon pulls, whose ready check waits on group mana.

## Gathering that never pulls

Herbing, mining and skinning between pulls is fine — dragging a pack
back to the group because a node was on the far side of it is not. A bot
skips a gathering node while it is in combat, while any group member on
the same map within 60 yards is in combat, and whenever an alive mob
that isn't already fighting sits within its own aggro radius (plus a 5
yard margin for approach error) of the node. The aggro test is the one
the dungeon follow spread uses for its standing spots, and it's applied
at the node, not at the bot, since that's where the pull would happen.
It runs both when a node is first noticed and again on the way over, so
a node that was safe when spotted is abandoned once a patrol wanders
past it. Skinnable corpses count as nodes: a corpse only becomes
skinnable once its regular loot is out, so picking up the kill itself is
never delayed — but going back to skin it through a respawn, or while
the group is still fighting, is. Applies in dungeons and outdoors alike;
no config option.

## World PvP excursions

Random bots occasionally travel to enemy or contested
towns (Southshore/Tarren Mill, the Crossroads, Stranglethorn — rarely even
enemy home zones like Goldshire) on purpose, lurk near town for 15-30
minutes picking fights, then go home. The trip teleports the bot to a spot
a couple hundred yards out (guarded so no real player sees the blink) and
walks it in PvP-flagged; the regular reactive PvP strategy produces the
actual fights — with none of the usual reluctance toward much lower-level
targets, since on an excursion ganking is the point. All levels
participate — overleveled "gankers" follow a level-gap curve rather than
being all 80s. Invaders goad unflagged enemies into attacking with rude
emotes; rogues/druids both go more often and deliver theirs by dropping
stealth right next to the mark, while Night Elves of other classes
Shadowmeld and hold the ambush instead. A bored invader — dwelling past
`AiPlayerbot.WpvpRaidBoredomSeconds` with no enemy player in sight — may
roll `AiPlayerbot.WpvpRaidChance` (once per excursion) for the classic
bait play: attack a nearby town guard it clearly outlevels so the
guard's death fires the faction-wide "... is under attack!" alarm and
defenders come to it (see the defense section below).
`AiPlayerbot.WpvpRaidFlightMasters` additionally allows the notorious
flight-master kill; it ships off, since a dead flight master denies the
taxi service to everyone for minutes.
Going home is literal: when the excursion ends — dwell expired (any
running fight is finished first), died too often, or the reported
attacker long gone — a bot that far outlevels the zone leaves for
level-appropriate grounds via the same guarded teleport, instead of
idling on the battlefield. Without that, contested leveling zones never
drain: ended excursions pile up as idle outleveled fighters who keep
brawling and re-answering every fresh callout, and the zone becomes a
permanent meat grinder. Bots within the zone's own level bracket (plus
the defense level slack) stay — they live there.
`AiPlayerbot.RpgStatusProbWeight.GoWpvp` sets the frequency (0 disables);
`AiPlayerbot.Wpvp*` knobs cover the rest; the `.playerbots wpvp` GM
commands ([below](#commands-added-in-this-fork)) provide a test hook,
observability, and a runtime kill switch.

## World PvP defense and reinforcements

Ganking has consequences, on both
sides. Defender bots call out invaders in LocalDefense ("Bloguk is
attacking Goldshire!"), throttled per zone and per attacker. A callout
takes evidence, not just presence — on a PvP-type realm everyone in a
contested zone is flagged, so an enemy quietly grinding mobs is everyday
leveling, not an attack. Bots report an enemy they see fighting the
town's guards and civilians; an enemy fighting a friendly player (or
their pet) only when that victim is genuinely outmatched — by level (a
full gank gap below the attacker) or by numbers (two or more enemies on
them), since an even scrap lost fair and square is a sight, not an
alarm, whether the victim is a bystander or the spotter itself; or a
still-fresh ganker the defense channels already named merely prowling
past — and the wording follows what was seen ("Bloguk is
attacking Elissa near Goldshire!" / "That ganker Bloguk is prowling
around Goldshire now."). An
uncontested *gank* spree — kills of victims at least the gank gap below
the ganker; even fights never escalate, no matter how many times somebody
loses one — earns one WorldDefense shout from an eyewitness: a victim of
the spree still in the ganker's zone, or a bystander with the ganker on
their screen, either way themselves a full gank gap below the ganker (a
faction-wide plea belongs to the genuinely outmatched, and it comes from
where the trouble is — a victim who released and left the zone is out of
the story). The zone-local callouts follow a softer
version of the same rule: a bot that outlevels the ganker by the gank gap
never calls out or pleads for help (it *is* the help), so arriving
high-level defenders fight instead of shouting about an enemy they could
squash. And *uncontested* is taken seriously, biased toward "this is
handled": any evidence that a capable defender — one within the gank gap
of the ganker — is on the case cancels the pending shout and resets the
spree, which must then be re-earned from zero. Evidence means a kill
scored by the defending side in the ganker's zone (in a pitched battle
both sides score constantly, so neither side's alarm fires — one channel
call, not a shouting match) or such a defender arriving to hunt the
ganker; a defender dying along the way is a normal part of defending, not
grounds for a fresh plea. Each faction shouts about a given zone at most
once per escalation window, so a battle full of mutual "ganks" produces
one call however many gankers it contains.
Behind the chat, idle bots across the faction may answer the call: each
rolls a small once-per-ganker chance to drop what it's doing, travel in
(arriving out of sight and walking the last stretch), and hunt the
attacker — staying on scene while the attacker is around, drifting home
once they're gone. Preying on much lower-level victims draws a stronger
response than an evenly matched brawl (`AiPlayerbot.WpvpGankLevelGap`
draws that line). The other side answers back too: a ganker who dies a
couple of times to outside help — killers who weren't among their victims
— may pull a one-time wave of faction allies as reinforcements, with no
chat involved (we assume the ask happened over some backchannel). All of
it keys off actual PvP kills, so a real player ganking lowbies is called
out, hunted, and — if the tables turn — reinforced exactly like a bot.
Three guardrails keep the answering from snowballing into an invasion of
its own. Waves are capped: per ganker, at most
`AiPlayerbot.WpvpDefenseResponderCap` defenders and
`AiPlayerbot.WpvpReinforcementCap` reinforcers actually set out — the
per-bot chance says how eager each bot is, the cap says how many the
battlefield can absorb, so wave size doesn't scale with the server's
idle-bot population. Waves are proportional: a traveling responder may
outlevel the threat by at most `AiPlayerbot.WpvpResponseLevelMargin`
levels — a level-30 skirmish draws 30s, not the faction's mains. For
defenders the threat is the reported attacker; for reinforcers it's the
strongest outside killer their faction-mate died to, at the level the
victim could read off the frame. The cap lifts when the fight genuinely
escalates: for defense, the WorldDefense "keeps killing people" plea; for
reinforcements, `AiPlayerbot.WpvpReinforcementEscalationDeaths` further
deaths after the first wave armed, which also opens a fresh wave's worth
of slots — the bracket-level friends weren't enough, now the mains log
on. (Explicit `!wpvp defend` orders and walk-up emote alerts ignore the
margin: an order is an order, and a passing main wading in is its own
classic story.) And responders stay on mission: a bot on a defense
trip never opens on a bystander a full gank gap below it (a lowbie
already fighting players is still fair game) — it came to stop a ganker,
not to start a spree of its own, and without that rule every defender
kill of a passing lowbie files the defender as a fresh "ganker" on the
enemy's board, waves answering waves forever.
`AiPlayerbot.WpvpDefense*` and `AiPlayerbot.WpvpReinforcement*` knobs;
the `!wpvp defend` chat command ([below](#commands-added-in-this-fork))
lets you order a defense yourself.
Town raids count as callouts too: the server's own faction-wide
"... is under attack!" broadcast (a guard dying to an enemy player,
scripted town alarms) files the attacker on the defense board exactly as
if a bot had shouted, so defenders may ride out even when nobody
witnessed anything — against raiding bots and raiding real players
alike. Attack a town and the militia actually comes. The broadcast names
only the place, so until an eyewitness files a real sighting the
attacker is assumed to be about the dead guard's level ("strong enough
to kill our level-25 guard") when deciding who responds — the first wave
may misjudge a much stronger raider, which is authentic. Bots never
repeat the broadcast in chat (the server already told everyone), and
NPC deaths never feed gank-spree escalation — WorldDefense pleas remain
earned by killing players. `AiPlayerbot.WpvpNpcAttackDefenseEnabled`
turns it off.

WorldDefense itself is made listenable: 3.3.5 clients treat the DBC
channel as unjoinable (its vanilla PvP-rank gate left with the old honor
system), so bots create it as an ordinary custom channel at login and the
core never force-joins players into it. `/join WorldDefense` works like
joining any player-made channel — join confirmation, `/chatlist` and
chat-pane listing, `/leave` — with a `channels_rights` row in the
characters DB suppressing join/leave announces and ownership.

A quieter signal rides the same hunt machinery: a targeted emote at an
enemy. When any friendly — player or bot — aims a text emote (`/point`,
`/charge`, nearly anything — sincere respect gestures like `/salute` and
`/bow` are exempt, see the [same-class truce](#same-class-truce) below;
a `/wave` or `/smile` at an enemy reads just as easily as gank taunting,
so those still count) at a PvP-flagged enemy player, every bot close enough
to have seen it (the `ListenRange.TextEmote` radius around the emoter)
treats it as "look at THAT one" and converges on the spot to hunt them.
No dice roll — the emote is the dice — so it reliably covers the enemies
bots fail to notice on their own: out of vision range, behind a wall, or
stealthed. Point at the lurking rogue and the bots around you fan out
after them. Bots already dwelling on a world-PvP excursion answer too
(they're out there exactly for these fights), while bots already answering
a defense call stay on task; the same level slack as defense responses
keeps hopelessly outleveled bots out of it, and the emoter itself never
counts as a witness — pointing is a callout, not a self-command to charge.
`AiPlayerbot.WpvpEmoteAlertEnabled` turns it off.

## Same-class truce

Some pairs of same-class enemies honor the old player code — "druids don't
gank druids". Where the regular world-PvP targeting would open an
unprovoked attack (or an excursion bot would goad), a truce-bound bot
declines, faces the spared enemy, and delivers a targeted `/salute`
instead — stealthed rogues and druids step out of stealth to do it, the
reveal being part of the gesture. Whether a given pair falls under the
code is decided deterministically per character pair, not rolled per
encounter: the configured percentage is the share of pairs that do, and
the decision never flips mid-standoff. Honoring it is personal, though — a
small deterministic share of individuals are oathbreakers toward a
particular rival, so most truce-bound pairs exchange salutes and move on,
but sometimes the recipient of the courtesy answers it with an attack.
The truce covers only unprovoked attacks:
a bot defends itself and assists its party exactly as before, instanced
PvP is exempt, and an enemy the defense boards already track as a ganker
forfeits the courtesy. Salutes are throttled per pair, and the emote-alert
machinery above deliberately ignores sincere respect gestures so the
salute never summons the militia onto the salutee.
`AiPlayerbot.WpvpClassTruceChance` sets per-class percentages
(default `druid:60,hunter:25`; unlisted classes never truce) and
`AiPlayerbot.WpvpTruceOathbreakerChance` the oathbreaker share
(default 15).

## Guard respect

A bot never opens or continues a world-PvP fight under the cover of
hostile guards that outlevel it by `AiPlayerbot.WpvpGuardRespectLevelGap`
or more (default 5, i.e. guards it has no answer to; 0 disables). An
enemy standing near such guards is refused as a target — even in
self-defense, where the human move at the guard line is to disengage,
not trade hits — and a chase after a fleeing enemy is broken off just
before the bot crosses into the guards' aggro bubble, instead of the
old behavior of blindly running past town guards way above its level
and dying to them. Excursion bots also won't goad an enemy their guard
bar would refuse to fight. Max-level gankers diving a town are
unaffected — no guard outlevels them by the gap — and instanced PvP and
duels are exempt.

## Terrain line of sight

The server's line-of-sight test only ray-casts model geometry — buildings,
caves, trees, gameobjects — and there is no config knob that adds more:
the terrain heightmap simply isn't part of it, so bots "saw" enemy players
straight through hills and ridges from up to `AiPlayerbot.WpvpVisionDistance`
(100 yards) away. In practice that meant a ganker instantly re-acquiring a
victim who resurrected just over a rise, out of any human's sight. Now
unprovoked target acquisition (the open-world enemy scan and party-assist
pickup) also samples the terrain heightmap along the sight line and refuses
targets the ground hides; a bump has to actually rise above eye line to
count, so gentle slopes don't grant cover. Self-defense is untouched —
a bot that's being attacked already knows where its attacker is — and
indoors the model geometry test already tells the truth. No config knobs.

## Level perception

The client refuses to tell you a hostile unit's level once it is 10 or more
levels above you: `UnitLevel()` returns -1 and the frame shows a skull, so
all a player learns from "??" is "at least ten above me". Bots read the
server's `GetLevel()` instead and knew the exact number regardless — a level
20 bot would call out "a level 80 undead rogue" over LocalDefense about a
ganker whose level it couldn't possibly have read.

Every comparison and every callout now goes through the same perception
rule: friendly units show their real level, hostile ones do too until they
reach the skull gap, and past it a bot gets nothing but the floor the skull
implies (its own level + 10). World bosses are "??" to everyone. Anything a
bot says about a level it can't read says "??" instead of a number, and the
shared defense board files levels as the reporting player saw them — a
relayed sighting never becomes more precise than the bot repeating it could
have seen for itself.

Bot decisions are unchanged: every level gate in the fork (the gank gap, the
defense slack, target selection's 5-level refusal) is smaller than the skull
gap, so a clamped level answers them exactly as the true one did. Only the
knowledge, and what gets said out loud, changed. Server-side telemetry still
logs true levels. No config knobs.

## World PvP threat reactions

Upstream bots are strangely oblivious to being attacked by an enemy player
in the open world; this cluster of fixes makes them react like people:

- **Eating and drinking**: a consuming bot used to sleep through its whole
  meal — the AI set itself a 10-27 second wake-up timer — so a ganker
  could kill it while it sat there. Bots now re-evaluate every second
  while consuming: still safe, keep sitting; attacked, or an enemy player
  closing within 40 yards, get up and respond. Upstream had that vigilance
  only inside battlegrounds; now it applies everywhere.
- **Long casts**: the same wake-up-timer pattern covered cast times, so a
  bot mid-Summon Imp (10 seconds) stayed committed no matter what.
  Entering combat now wakes the AI immediately, and a non-combat cast is
  interrupted — unless it's nearly finished (under 3 seconds left), in
  which case finishing it is the better play.
- **Target priority**: the bot's attacker list was built from creature
  threat lists, which never contain players — an enemy player beating on
  a bot busy fighting mobs literally did not exist to its target
  selection. Open-world PvP assailants now enter the attacker list and
  outrank whatever mob the bot was already fighting. (Battlegrounds keep
  their own targeting machinery.)
- **Soulstone discipline**: a warlock's soulstone (or a shaman's
  reincarnation) fired the instant the bot died, even mid-gank — handing
  the enemy a free second kill and wasting the stone. Bots now hold the
  self-res while a PvP-flagged enemy is within 40 yards, waiting up to
  twenty seconds for them to leave; if the enemy stays, the bot uses the
  self-res anyway — resurrecting into a camper beats releasing and losing
  the stone.
- **No rezzing into campers**: a dead bot ran back and popped up at its
  corpse — or took the spirit-healer res — regardless of who was standing
  on it. Ghosts now wait out a flagged enemy loitering at the rez spot,
  and only after about ninety seconds of being camped give up and rez
  anyway.
- **No dueling next to a battlefield**: idle bots would happily start a
  recreational duel while a gank unfolded across the road. Bots now
  neither offer nor accept another bot's duel while world PvP is live
  nearby — a PvP kill or defense callout in their zone within the last
  two minutes, or a PvP-flagged enemy player inside vision range
  (`AiPlayerbot.WpvpVisionDistance`). A real player's challenge is still
  accepted; a human read the room.

No config knobs; the thresholds (40 yards, twenty seconds, ninety
seconds, two minutes) are fixed.

## Corpse-camping satiation

The killer-side counterpart to "no rezzing into campers" above. Bots have
no natural attention span: one loitering near your corpse — excursion bots
deliberately dwell near their anchor for 15-30 minutes — would re-kill you
on every rez until its dwell expired, with inhuman patience. Now each
world-PvP kill rolls a satiation chance for the killing bot: on success it
considers that particular victim dealt with and stops initiating attacks
on them for a grace period, going back to whatever it was doing. Fighting
back is untouched — attack a satiated bot and it defends itself as usual.
Because every kill re-rolls the same dice, the drop-off is geometric: at
the default 60% chance about 40% of bots re-kill once, 16% twice, 6%
three times. Most move on after a kill or two; the rare persistent camper
survives as a realistic griefer, not the norm.
`AiPlayerbot.WpvpSatiationChance` (percent, 0 disables) and
`AiPlayerbot.WpvpSatiationMinutes` set the dice and the grace.

## Grudges and mercy

The victim-side counterpart to satiation. A bot killed in a fair
world-PvP fight used to rez and serenely resume grinding its quests with
its killer standing right there — even watching them fight someone else.
No player forgets a death that fast. Now a killed bot holds a grudge
against its killer for `AiPlayerbot.WpvpGrudgeMinutes` (default 15), in
one of two dispositions rolled at death:

- **Revenge** (`AiPlayerbot.WpvpRevengeChance`, default 60%): the killer
  becomes an attack-on-sight priority target. Sighting them skips the
  usual courage dice, pulls harder on target selection than any other
  attraction on the field (so a rezzed bot joins the killer's current
  brawl and focuses *them*, not the nearest stranger), engages at full
  vision range, and voids any same-class truce with them. Sanity lines
  stay: guard respect still applies, and a killer who plainly outclasses
  the bot (the "don't attack a much higher level" cap) never inspires
  revenge in the first place. Killing the killer settles the grudge.
- **Avoidant** (the failed roll, a hopelessly outleveled killer, or a
  *second* death to the same killer within the window — losing the
  rematch teaches the lesson): the bot wants nothing more to do with
  them. It never initiates against the killer — not even to join a
  passerby brawl the killer is part of — and when they come within ~40
  yards it faces them, pleads (a targeted `/shoo`, `/beg`, or `/cry`),
  and retreats, fleeing again each time the killer keeps closing.
  Fighting back when actually attacked is untouched.

The pleas are a real mechanic, not just theater: a `/beg`, `/cry`, or
`/shoo` by anyone being attacked by a bot — or aimed at a bot stalking
them — rolls `AiPlayerbot.WpvpBegMercyChance` (default 15%) per bot to
move it to mercy. A merciful bot breaks off and leaves the beggar alone
until the beggar swings at someone (begging then fighting forfeits the
grace) or several minutes pass. The roll is deterministic over a short
window, so emote spam doesn't reroll it; real players' pleas work on
bots exactly the same way, and an avoidant bot's begging can genuinely
wave off a bot killer. Plea emotes are exempt from the
[emote-alert rally](#world-pvp-defense-and-reinforcements) — waving a
fight off must not double as a silent callout. `AiPlayerbot.WpvpGrudgeMinutes = 0`
disables grudges; `AiPlayerbot.WpvpRevengeChance = 0` makes every grudge
avoidant.

## Vendettas

The grudge above is a fifteen-minute reflex that evaporates on restart.
Underneath it sits a persistent ledger in the playerbots database: every
*unprovoked* world-PvP death a bot suffers is tallied per killer, across
sessions, forever. Deaths in fights the bot itself picked — a revenge
sortie, a courage-dice initiation, riding out to a defense call — never
count: losing a fight you chose breeds no resentment, being hunted does.
Enough ganks (`AiPlayerbot.WpvpVendettaGanks`, default 3 — or
`AiPlayerbot.WpvpVendettaCampGanks`, default 2, when any were re-kills
within ~10 minutes of the last, i.e. camping) open a **vendetta**.

An open vendetta acts like a revenge grudge that never expires: attack on
sight past the courage dice, satiation, and same-class truces, at full
vision range — except while the killer still plainly outclasses the bot,
when it reads as fear instead (avoid, plead, retreat, like an avoidant
grudge). The disposition is derived at the moment of sighting, so a bot
repeatedly camped at low level by someone far above it fears them for as
long as the gap holds — and comes for them once it has leveled into
range. The fresh reflex grudge always speaks first; the vendetta fills in
when no short-term memory stands.

Killing the offender settles the vendetta — the bot stops hunting, but
the tally survives, and a single fresh gank re-opens it. Fleeing and
pleading can settle it too: if the bot's last encounter with the
tormentor ended in flight and pleas and no re-kill followed within ~5
minutes, the vendetta is forgiven at the next sighting — the griefer
stopped their behavior, and the bot lets it go (until the next gank). There is no
settling exemption in the ledger itself: a successful revenge kill is
still an unprovoked death from the other side's point of view, so two
bots can absolutely end up in a mutual, self-sustaining feud — true to
world-PvP life. Vendettas form against real players and bots alike (only
bots *hold* them; a human's resentment is their own), and the
`playerbots_wpvp_vendetta` table doubles as a browsable history of who
griefed whom. `AiPlayerbot.WpvpVendettaGanks = 0` disables vendettas;
`AiPlayerbot.WpvpVendettaCampGanks = 0` removes the camping accelerator.

## Chase break-off

Nothing in the pursuit path had a distance or time bound, so a bot whose
world-PvP target ran would chase them across the continent with Terminator
single-mindedness — the only things that ended a chase were a kill, the
bot's death, or guard cover. Now a chase whose target has broken contact
(no damage in either direction for a few seconds and beyond ~30 yards)
rolls `AiPlayerbot.WpvpChaseBreakChance` to give up after each interval of
`AiPlayerbot.WpvpChaseBreakSeconds{Min,Max}` (randomized per roll). Like
satiation, the repeated roll makes the falloff geometric: at the default
50% half the bots shrug and turn back within one interval, three quarters
within two, and the rare dogged one hunts you far longer — give-up timing
varies instead of every bot leashing at the same beat. Landing a hit or
closing back into range resets the clock, so an actual running battle is
never interrupted; only a cleanly escaping target gets released.

Giving up sticks: the bot won't re-acquire that runner on sight, so
stopping to eat at 70 yards doesn't restart the hunt. The grudge is
behavioral, not timed — it clears the moment the runner turns the fight
back on, by closing meaningfully back in on the bot or by landing a hit on
anyone (a returning entrance invites a re-chase, and swinging at the bot's
group mate makes them fair game again). As a backstop, a bot stuck in any
unmastered combat for five minutes now genuinely drops its target — the
upstream trigger for that pointed at an action that was never registered.
`AiPlayerbot.WpvpChaseBreakChance = 0` disables the whole leash.

## Target peeling

Once a bot committed to one enemy player it was locked on: the target
value that feeds combat never re-evaluated, so a bot would chase its
chosen victim straight past a fresh enemy standing on top of it. Now a bot
fighting one enemy player in the open world switches to another who shows
up at least `AiPlayerbot.WpvpPeelAdvantageYards` (default 25) closer than
the current fight — close threats beat a receding chase, and the margin is
wide enough that the switch reads as opportunism rather than indecision
(no ping-ponging between two similar targets). Battlegrounds keep their
own targeting machinery. Alongside this, enemy sightings are evaluated
best-first by the kill-the-add score described under [passerby
assist](#passerby-assist) (the scan used to take whichever acceptable
enemy the grid happened to list first); the peel margin compares the same
score, so switching and sighting can never disagree. `0` disables
peeling.

## Shaman combat overhaul

Shamans in PvP mostly stood around auto-attacking and Purging: the Purge
trigger fired every second on *any* dispellable magic buff at a priority
above the entire damage kit, Frost Shock was dead code (registered but
never wired into a strategy), Hex didn't exist, and the totem loadouts
were hardcoded per spec with no Grounding or Earthbind ever. Now:

- **Purge discipline** — Purge only fires on buffs worth the global
  cooldown, checks every 5 seconds instead of every second, and is
  skipped below 40% mana. This became the shared offensive-dispel rule
  for every class, see [PvP fundamentals across
  classes](#pvp-fundamentals-across-classes).
- **Frost Shock** — snares enemies fleeing or chasing an ally (the
  hunter Concussive Shot pattern), and elemental/resto keep it on a
  hostile player target to kite.
- **Instant fillers while moving** — a moving caster shaman falls
  through to instant Flame Shock / Frost Shock / Earth Shock instead of
  auto-attacking (the engine already refuses cast-time spells while
  moving; the gap was that nothing instant was queued). The `moving
  filler` trigger behind it is generic, so any class can hang its
  instants off it.
- **PvP totems** — versus player opponents (battlegrounds, world PvP,
  and duels alike — detected from the actual combatants, not the map),
  Grounding Totem owns the air slot and Earthbind the earth slot,
  reverting to the equipped loadout against creatures.
- **Hex** — shamans take a secondary attacker out of the fight with the
  same crowd-control targeting mages use for Polymorph.
- PvE loadout fixes: enhancement drops Searing instead of Magma (which
  is useless outside melee range), elemental drops Strength of Earth
  instead of Stoneskin.

## PvP fundamentals across classes

An audit of the other classes found the shaman's problems were the rule,
not the exception, and that most of them sat in machinery every class
shares rather than in any one rotation. The shared fixes:

- **Dispel policy** — `HasAuraToDispel`, behind every dispel trigger,
  now decides whether an aura is worth a cast. Offensive dispels
  (Purge, Spellsteal, Devour Magic, Tranquilizing Shot) only fire on
  enrages, absorb shields, HoTs, and a shortlist of major cooldowns
  (Bloodlust/Heroism, Earth Shield, Hand of Freedom/Protection/Sacrifice,
  Inner Focus, Power Infusion, Pain Suppression, Icy Veins, Arcane Power,
  Avenging Wrath, Nature's Swiftness), check every 5 seconds, and are
  skipped below 40% mana. Defensive dispels stay unfiltered for healers;
  a non-healer facing a player opponent only cleanses what takes a
  teammate out of the fight (CC, roots, snares, silences, disarms) and
  leaves DoTs and stat debuffs to the healer. Druid, paladin, and priest
  cleanses also move below critical heals in priority, so a dying tank
  outranks a cure.
- **Crowd control without a raid icon** — druid Cyclone / Hibernate /
  Entangling Roots and warlock Fear / Banish used to wait for a
  moon-marked target, which the auto-marker never sets on players or in
  battlegrounds. With no icon they now use the same secondary-attacker
  selection Polymorph and Hex use (a set icon still wins).
- **Crowd control works solo** — the shared CC target selection required
  a group (its ranking measured distance from the group's tanks), so an
  ungrouped bot never used any of the above. Solo with several attackers
  it now controls the most dangerous one that isn't the current target:
  a player over a mob, a mana user (healer or caster) over a melee, the
  farther one when tied. Alone against a single player and losing
  (health below `AiPlayerbot.MediumHealth` or mana below
  `AiPlayerbot.LowMana`), the opponent themself is controlled to reset
  the fight — the CC drops the target, PvP combat lapses, and the bot
  heals, bandages or drinks. A healthy bot does not open with CC from
  this path; setups like Polymorph into Pyroblast belong to the class
  strategy that knows the follow-up. Every candidate is skipped if it is
  already controlled, below `MediumHealth` (the regenerating time-out
  would be a gift), inside the bot's own AoE, or — for players — at
  diminishing-returns level 3 or immune for that spell's DR group.
- **Bots don't break crowd control** — a unit under a damage-breakable
  control effect (Polymorph, Hex, Sap, Gouge, Repentance, Freezing Trap,
  Hibernate, fears; roots excluded so kiting keeps working) leaves the
  bot's attacker list, invalidates its current target even when that is
  the enemy player it is fighting, and is never re-acquired by the PvP
  targeting until the effect ends. Previously only Polymorph was
  handled, so a shaman Hexed and went on casting at the frog.
- **Snares see players** — the shared snare targeting only understood
  NPC chase/flee movement, so Concussive Shot, Hamstring, Piercing Howl,
  Thunder Clap, Shockwave, Intercept, Hammer of Justice, and Frost Shock
  never snared a real player. A hostile player that is moving and not
  already rooted or slowed now qualifies, the bot's current target
  first.
- **Escapes fire against player melee** — the "enemy too close" triggers
  behind mage Blink / Dragon's Breath, hunter Disengage, and caster flee
  were suppressed whenever the enemy was attacking the bot (sensible
  against a mob that will follow anyway). A player or player pet in
  melee on the bot now counts, so casters actually use their escape
  tools.
- **Threat drops without a main tank** — the "medium threat" trigger
  behind priest Fade, warlock Soulshatter, cat Cower, hunter Feign
  Death, rogue Vanish and mage Invisibility required a designated main
  tank, which never exists solo, in a tankless party, or in an arena.
  It now fires when two or more creatures are on the bot and each has
  someone else on its threat list to fall back to (a tank, any party
  member, the bot's pet). Player-controlled attackers are not counted:
  a threat drop does nothing against a player.
- **Escapes in PvP** — a new "pvp escape" trigger fires when two or
  more enemy players (or their pets) are on the bot, or one is while
  the bot is below `AiPlayerbot.LowHealth` and its opponent is not
  lower still. Rogue Vanish and hunter Feign Death hang off it; mage
  Invisibility does not, since the bot's next cast would break it.
- **Priest, paladin and rogue get their CC strategy** — the combat
  engine never enabled `cc` for these three classes, so Shackle Undead,
  Turn Evil and Sap were unreachable however they were wired. Paladin
  Turn Undead was also still addressed by its pre-3.0 name, which
  resolves to no spell; it is Turn Evil now. Sap itself is handled by
  the rogue's stealth opener below, since the shared CC target selection
  only scans attackers and Sap needs an out-of-combat target.
- **Burst cooldowns on a dying player are not burst** — the shared
  "boost" trigger treated any player target as a reason to pop every
  major cooldown (Avenging Wrath, Icy Veins, Metamorphosis, Adrenaline
  Rush, racials, trinkets, …). A player already below
  `AiPlayerbot.LowHealth` no longer qualifies on their own: the rotation
  finishes them and the cooldown stays up for the next opponent. DK
  Army of the Dead, a 4.5-second channel that used to open every PvP
  fight, is additionally held back against players unless the bot is
  grouped or has two or more attackers on it.
- **No more silent dead code** — a strategy that references a trigger or
  action name with no registered factory now logs a warning the first
  time it is seen, instead of silently skipping the node.

- **Usable-while-controlled spells** — the cast check refused every
  spell while the bot was stunned, feared or confused, so the abilities
  that exist for exactly that moment (PvP trinkets, Every Man for
  Himself, Will of the Forsaken, Berserker Rage, Icebound Fortitude,
  Lichborne, Barkskin, …) only worked where a hand-written exception
  existed. Spells the client allows while stunned / fleeing / confused
  now pass; jumping and charging still block everything.
- **Intervene and Hand of Protection** — the shared "party member to
  protect" value had an unconditional early return from an upstream
  optimisation pass, so neither ever fired. Restored.
- **Snares on a dying target** — snares inherited the "target must live
  another 8 seconds" rule meant for DoTs, so Hamstring, Chains of Ice,
  Deadly Throw and company were refused on the one target most likely to
  run. Snares are exempt.
- **Stealth openers everywhere** — the rogue `stealth` and druid
  `prowl` non-combat strategies were only enabled in battlegrounds.
  Every rogue and every druid now has them in the open world too, so
  Sap, Cheap Shot, Ambush, Pounce and Ravage are reachable in world PvP
  and while grinding; the trigger already waits for a target within 30
  yards, out of combat and with Stealth off cooldown. Druids of any
  spec sneak the way they already do in battlegrounds — the prowl
  action's Cat Form prerequisite shifts a caster or bear build in, and
  the combat engine puts it back in its own form once the fight starts.
  The one exception is a grouped bear-build tank, which stays visible
  so it never wanders off stealthed ahead of a pull (solo tanks still
  prowl).
- **PvP trinket on loss of control** — the factory already equips a
  Medallion / Insignia on PvP-spec bots from level 50, but no trigger ever
  used it: the only path to an equipped trinket was the generic
  cooldown-burn on a healthy target, which fired the Medallion while the
  bot was free and, because it tries slot 1 first, kept the real DPS
  trinket in slot 2 from being used on the same tick. A dedicated `use
  pvp trinket` action now fires at emergency priority whenever the bot is
  stunned, feared, rooted, confused, charmed or asleep, bypassing the
  normal cast check like the racial breakers do; the generic burn leaves
  PvP trinkets alone. The cast check itself also learned the core's
  rule that a spell purging the controlling mechanic (the trinkets,
  Every Man for Himself) is castable under it, instead of relying on
  client attribute bits those spells don't carry.

## Per-class PvP passes

With the shared machinery in place, each class got a pass for the three
kinds of rot the audit kept finding: nodes wired to names that resolve to
nothing (typos, pre-3.0 spell names, factories never registered),
abilities that were implemented correctly and referenced from no
strategy, and kits that made sense against mobs but not against a player
who moves, heals and casts. Casters also get instant fillers while
moving (the `moving filler` trigger introduced with the shaman), so a
kited caster presses something instead of auto-attacking. By class:

- **Death knight** — Mind Freeze (and the enemy-healer variant) move up
  to interrupt priority; Strangulate, a 30-yard silence that was gated
  as a melee action, backs it up. Chains of Ice, registered and never
  wired, snares the shared snare target and keeps a kiting player
  slowed. A player target out of melee draws Death Grip, with Death Coil
  filling the chase (frost had no ranged instant at all). Anti-Magic
  Shell is a self-buff against an attacker casting at the bot, Icebound
  Fortitude also answers a stun, Lichborne answers fear/charm/sleep, and
  frost uses Hungering Cold when two or more players stand within 10
  yards. Frost and unholy sit in Unholy Presence against player
  opponents and Blood Presence otherwise. Unholy Blight (a passive in
  3.3.5) and two mistyped registration keys are gone.
- **Druid** — Entangling Roots kiting (registered, never wired) roots a
  mana-less player who closed on a balance or resto druid. A new "shift
  to break snare" trigger cancels form when a cat or bear is rooted away
  from its target, or a moonkin has a player in melee, and the spec's
  form trigger shifts straight back. Maim is the cat interrupt, Barkskin
  also fires below `MediumHealth`, flag-carrying balance/resto druids
  take Travel Form, and the Cyclone / Hibernate / Roots CC nodes move
  below critical heals now that they actually fire on players. Ferals
  also heal themselves mid-fight now — nothing ever fired a self-heal
  while the bot was in cat or bear form, so a feral at critical health
  drops form for a Regrowth (stacking an instant Rejuvenation on top if
  it stays critical while the Regrowth HoT ticks), a cat cashes a
  Predator's Swiftness proc in for an instant Healing Touch already at
  low health, and either shifts straight back. Frenzied Regeneration
  moves above movement priority so a bear in a PvP fight actually
  presses it instead of forever repositioning.
- **Hunter** — `explosive shot` was an unregistered name, so survival's
  signature shot only fired off Lock and Load; Arcane Shot's guard was
  inverted (only survival used it). Freezing Trap, Scare Beast and
  Wyvern Sting go through the shared CC targeting. A player swinging at
  the hunter gets Freezing Trap at feet → Scatter Shot → Frost Trap →
  Disengage, Master's Call clears snares, marksman Silencing Shot also
  hits the enemy healer, and Aspect of the Viper swaps at
  `AiPlayerbot.LowMana` rather than ~7%.
- **Mage** — Counterspell on the current target (trigger and action
  existed, wired nowhere). Presence of Mind is cast from the boost
  strategy and spent on Pyroblast, Arcane Blast or Frostbolt. Fire and
  frostfire open an even solo 1v1 with Polymorph → Pyroblast — both at
  `AlmostFullHealth`, one attacker, Polymorph not diminished — the
  Pyroblast node sitting just above the shared drop-target reaction that
  fires the tick the sheep lands. Frost Nova answers any attacker within
  10 yards, Ice Lance is the shatter fall-through, arcane gets Slow for
  kiting, and Mirror Image hangs off the working "medium threat" trigger
  instead of the unregistered "high threat".
- **Paladin** — `blessing of protection` is a pre-3.0 name resolving to
  no spell, which killed the emergency party save; it is Hand of
  Protection now. Repentance was fully dead and is crowd control via the
  shared CC target plus the backup interrupt for both ret strategies.
  Judgement of Justice caps a kiting player's run speed, Hand of
  Sacrifice covers a party member at critical health, and a healer holds
  Divine Plea while anyone needs healing.
- **Priest** — Psychic Scream (registered, never wired) fires on a player
  in melee, with Psychic Horror one tier lower for shadow; both are held
  for players since they break on damage and scatter mob packs. Inner
  Fire is refreshed in combat, Fear Ward is kept up and re-applied
  against player opponents, Binding Heal is wired, Shadowfiend hangs off
  its real trigger, and the holy DPS list no longer ends in Starshards
  (removed in 3.0). Power Infusion was wired to the priest itself and
  nowhere else; it goes to the group's caster dps first, through a new
  "party member to boost" value that wants an in-combat mana caster
  within 30 yards, free to cast (not sheeped, feared, stunned or
  silenced) and not already under Power Infusion, Bloodlust or Heroism.
  The self-buff is the fallback when there is nobody to hand it to.
- **Rogue** — `blade flurry` was registered as "blade fury". Kidney Shot
  on a player target with three or more combo points, Dismantle against
  melee players, Gouge behind Kick → Kidney Shot, Vanish → Cloak of
  Shadows → Blind on "pvp escape". The new "sap opener" has a stealthed
  rogue committed to one player Sap a second enemy within 10 yards so
  the opener lands 1v1. Shadowstep outranks Sprint on a runner, Deadly
  Throw snares one, Expose Armor waits for five combo points, and
  rogues expecting players carry Crippling Poison on the off-hand.
- **Warrior** — the stance-requirement factory was defined but never
  registered, so every stance-gated ability failed its cast check; it is
  registered and the stance switches are alternatives rather than
  prerequisites (the engine only pushes prerequisites once the action is
  already possible). Arms gets Pummel behind a cooldown gate so it only
  dances out of Battle Stance when the interrupt is up, Intercept on a
  runner (its trigger name was misspelled), Heroic Throw and Shattering
  Throw (whose immunity check looked for the pre-3.0 Blessing of
  Protection). Arms Hamstring and fury Piercing Howl → Hamstring snare
  the shared snare target; Intimidating Shout moves to "pvp escape",
  Enraged Regeneration to low health, Retaliation to "being attacked".
- **Warlock** — `devour magic cleanse` cast a spell literally named that
  (id 0) at the enemy; it cleanses a party member now. Conflagrate and
  Chaos Bolt hung off unregistered triggers and sat at filler priority.
  Howl of Terror and Death Coil answer a player in melee, Shadow Ward
  moves out of the never-enabled "tank" strategy onto the shared deflect
  trigger, and Drain Life fires at critical health.

## Initiation readiness

Bots used to start open-world fights at any state of their own bars — a
mage at half mana would jump a flagged enemy it had no business engaging.
Before an unprovoked pick (the sighting scan and the stealth goad), a bot
now weighs the fight the way a player would — "more advantage now, or
after I'm full up?". Below `AiPlayerbot.WpvpInitiateSelfHealth` percent
health (default 80) or, for mana classes,
`AiPlayerbot.WpvpInitiateSelfMana` percent mana (default 60), it passes —
unless "now" is clearly better: the target is already trading blows with
other players, reads 5+ levels below the bot, or is the bot's revenge
grudge (revenge doesn't wait on a drink). A bot that passes sits down to
drink or eat while the opportunity is still warm, so what you see is a
mage eyeing you over its water — the existing consumable safety guard
keeps it from doing that within 40 yards of an armed flagged enemy.
Self-defense, party assists and battlegrounds are never gated: nobody
checks their mana bar when jumped. `0` disables either bar. Stand-downs
are logged as `wpvp_initiate_gated` events.

## Passerby assist

On a PvP world it's rare for anyone to ignore a fight playing out in
front of them — etiquette demands jumping in. An already-flagged solo bot
that sees an enemy attacking a faction-mate within
`AiPlayerbot.WpvpPasserbyAssistRadius` (default 40) yards joins the fight
at `AiPlayerbot.WpvpPasserbyAssistChance` (default 90%), decided
deterministically per bot/enemy pair so the choice holds instead of
flickering. Joining an ally's fight skips the usual courage gates: even
an enemy up to the "??" line gets jumped, and an underleveled enemy
already fighting players draws no reluctance dice — they chose to be a
combatant. Two hard lines remain: an unflagged bot never joins (etiquette
does not demand flagging yourself for fights you happen to see), and a
skull-level ("??") enemy is a massacre rather than a fight — the bot
won't attack, though healer classes may still support the victim (see
[bystander assist](#bystander-assist)).

The other side of the same etiquette: an underleveled helper who joins a
fight between equals is commonly targeted and killed first, like an add
in a dungeon. Open-world target selection scores enemies instead of
taking the nearest — an enemy perceivedly below the bot reads about four
yards closer per level, and one already trading blows with players closer
still — so the lowbie who piles in gets focused down. Because everyone
watching saw a battle rather than a gank, a joiner who dies as the add
doesn't feed the killer's gank-spree tally, and no WorldDefense
escalation arms over adds killed mid-brawl.

## Duel openers

A bot that agreed to a duel just stood in the open and traded first hits
when the count reached zero. Bots now use the 3-second countdown the way a
player would, by class:

- **Rogues** restealth (the periodic out-of-combat fidget that used to
  strip stealth right back off now leaves duelists alone); **feral
  druids** shift to cat form and Prowl. Either side of the challenge opens
  its duel from stealth — and since stealthing point-blank in someone's
  face just shows them the shimmer, the stealther then slips out of the
  opponent's detection ring for the rest of the countdown and circles
  behind once the duel begins (see
  [Stealth flanking](#stealth-flanking)).
- **Warriors** back off to Charge range.
- **Hunters** drop a Freezing Trap at their feet, then step out past the
  minimum ranged-attack range — the opponent crosses the trapped starting
  spot on the way in.
- **Casters** (mages, warlocks, priests, elemental/resto shamans,
  balance/resto druids) make room for an opening cast.

Opening distances are measured from the duel flag — the midpoint of the
duel — so two repositioning duelists add their preferred ranges instead of
chasing each other's backpedal, and everyone stays well inside the duel
bounds. Paladins, death knights, and enhancement shamans open from where
they stand. No config knob.

## Stealth flanking

A stealthed bot used to run straight at its target's face, which defeats
the point: stealth detection is frontal-only and reaches roughly 9 yards
at equal level, so the target watches the shimmer walk the whole way in.
Now a stealthed rogue or prowling feral closing on a player who hasn't
engaged it does the footwork a real one would: it backs out of the
target's detection ring if it starts inside, circles around the ring's
edge across the front arc (behind the 180° arc a stealther is invisible
at any range), then walks straight in on the target's back for the
Ambush/Cheap Shot/Pounce opener. The ring is computed with the same math
the server uses for real detection — level difference plus stealth and
detect auras — so a Subtlety rogue hugs a tighter circle and a
higher-level target forces a wider one, and a target with outright
stealth detection up (a hunter's Flare) can't be flanked at all. Bots
also hold their auto-attack swings while stealthed, so arriving in melee
range no longer spends the opener on a white hit. The behavior is the
same wherever stealth happens: duels (where the countdown back-off above
feeds straight into it), battlegrounds, and world PvP. Along the way this
fixes an upstream bug where the rogue's stealthed-opener combat strategy
never activated at all — nothing ever swapped it in when a rogue entered
a fight stealthed. No config knob.

## The Distract trick

Rogue bots that would otherwise flank (above) can spend Distract the way
a practiced player does: the point lands past a target that's facing the
bot, the forced turn puts the target's back squarely to it (Distract's
server effect works on out-of-combat players, and stealth detection is
frontal-only), and the flanking logic's walk-straight-in branch takes
over from there — no circling. Not every rogue plays this way, and the
ones that do pick their moments. Whether a rogue knows the trick at all
is a stable per-character roll (`AiPlayerbot.RogueDistractChance`,
default 60% — 0 disables), so a given rogue is consistently tricky or
consistently not. A tricky rogue only casts it at full energy (the 30
energy comes out of overcap during the walk-in, never out of the
opener), only against targets that con yellow, orange, or red (a green
or gray mark isn't worth outplaying), and never against a defender with
area damage or detection already deployed — Consecration or Death and
Decay glowing on the ground, a flare, an armed hunter trap (visible to
rogues through Detect Traps), or a fire totem — since those sweep the
approach no matter which way their owner faces. PvP only: against
creatures, sneaking is free and energy isn't.

The trick has a counter, because a real player who gets Distracted knows
exactly what the forced turn means. A bot on the receiving end rolls per
incident (`AiPlayerbot.DistractSuspicionChance`, default 60% — 0 makes
Distract always work): a failed roll stares at the noise for the full
duration, a passed one — after a jittered human-scale beat of 1–2.5
seconds — breaks the facing lock, spins around, and treats the opposite
lane as a stealth suspicion for the flushing machinery above, sweeping
it with Consecration, a flare, an AoE pulse, or a walk-over search per
its class (`StealthFlushChance` and `StealthFlushSeconds` apply as
usual). Everything the defender does derives from its own forced facing
— it never learns who threw the noise, and the suspicion carries no
identity. The two systems feed each other: a sharp defender's flush on
the lane is precisely the deployed area damage that stops a tricky
rogue from trying Distract again. The reaction delay is what keeps the
gamble honest — a Distract thrown from inside ~12 yards usually still
buys the opener before the spin; one thrown from 20+ gets punished.

## Duel consumable etiquette

Classic dueling culture has an unwritten "no pots" rule, and upstream bots
break it freely — chugging a healing potion at critical health and a mana
potion at 40% mana, duel or no duel. `AiPlayerbot.DuelConsumables` sets
what bots allow themselves while a duel is in progress: `0` = no
consumables, `1` = bandages and engineering items only, `2` = potions and
everything else (the default, matching upstream behaviour). We run `1` on
Felworld — bandages were the one consumable everyone's duel circle seemed
to permit. The setting covers every duel a bot fights (bot or real
opponent) and has no effect outside duels.

## Bandage crafting

Bots stock their own bandages. Upstream bots love *using* bandages — the
combat logic reaches for one whenever things get dicey — but nothing ever
put bandages in their bags: bots never craft anything on their own, and
the bot outfitter that hands out potions, food, and ammo skips bandages
entirely. Now a bot with First Aid, when idle and out of combat, crafts
bandages from the cloth it's carrying (quested, looted, whatever) up to a
stock of 20 — real casts from its own spellbook, so the cloth is consumed
and First Aid skills up naturally, and custom recipes work because
nothing is hardcoded. Tier hygiene is enforced on both ends: recipes the
bot has outgrown (gray) are never crafted unless they're the best it
knows (so a skill-capped bot still restocks, but a level 55 never burns
linen on useless bandages), bandages below the tier it can now make are
tossed rather than hoarded, and when actually bandaging, every bot uses
the best usable bandage in its bags instead of whichever one it finds
first. Always on for bots with First Aid; no config knobs.

## Engineering in combat

Engineer bots fight like engineers. Upstream bots pick professions and
level them, but engineering never showed up in a fight — no bombs, no
gadgets, none of the toys that make the profession worth playing.
Engineering is one of only two professions in the game whose crafted
consumables are locked to the profession (First Aid is the other), so
this is the one place profession-flavored combat behavior is actually
realistic rather than cosmetic.

The bot outfitter stocks engineer bots by skill, mirroring how it hands
out potions: the best thrown explosive their skill allows (dynamite,
bombs, grenades — classified from the items' on-use spells, so every
tier from Rough Dynamite to Saronite Bomb is covered without an ID
table), a stun grenade when the main explosive doesn't stun, a sapper
charge, target dummies, Goblin Jumper Cables, and an explosive sheep or
two. Outgrown tiers are evicted at maintenance, same as bandages. At 375
skill bots also get tinkered: Nitro Boosts on boots, and Hyperspeed
Accelerators (most) or a Hand-Mounted Pyro Rocket (some) on gloves,
applied after regular enchanting so the tinker wins the slot — the
authentic engineer choice.

In combat, the new `engineering` strategy (on by default, including
battlegrounds but not arenas) puts it all to use. Bots throw their best
explosive at players, elites, and multi-mob pulls — with an occasional
lob at an ordinary mob just because — and answer an enemy cast with a
stun grenade at just-below class-interrupt priority, so a real kick wins
when one is available. Surrounded by three or more attackers with a
healthy HP buffer, an engineer sets off a sapper charge; overwhelmed and
hurting, it drops a target dummy to shed aggro — but only outside
dungeons and raids, and only if at least one attacker is something a
taunt can move, since players and their pets have no threat list and
ignore the dummy outright; now and then it releases an explosive sheep.
Glove tinkers fire on cooldown. Rocket boots pop
when it matters most: carrying a Warsong Gulch flag, chasing an enemy
flag carrier who is pulling away, or fleeing below 25% health. A
stealthed bot holds every gadget — an item cast would knock it out of
stealth, and no gadget is worth spending a stealth opener or a vanish
on. And after
the fight, a bot with no real resurrection spell but a set of jumper
cables will walk to a dead group member and try a jump-start — the
item's native fail chance supplies the comedy. Duels honor
`AiPlayerbot.DuelConsumables` (engineering items are tier `1`, the
Felworld setting). No other config knobs.

## Outlived immunities

Bots never cancelled an immunity: a mage that Ice Blocked at critical
health stood frozen for the full ten seconds after the healer had long
topped it up, a paladin kept a bubble that halves its damage and makes
every mob walk past it (the core suppresses damage-immune victims on the
threat list, so a bubbled tank hands its pulls to the healer), and a bot
under a human's Divine Intervention sat out the full three minutes after
the mobs had reset instead of resurrecting the group. Upstream's
`RemoveAuraAction` only existed as the `ra` chat command, plus a few
hand-placed removals in raid scripts.

The always-on `cancel immunity` strategy (combat and non-combat engines,
battlegrounds included) drops an aura the way a player right-clicks it
off, so it works from inside the stun. It needs two *positive* reasons
before it acts - an immunity is never dropped merely because the
emergency that called for it has passed:

1. **The aura is holding the bot back.** Ice Block and Dispersion lock
   the bot out of acting (Dispersion counts as limiting only once mana is
   back above `AiPlayerbot.MediumMana`; below that it is still doing its
   job as a mana battery). Divine Shield halves damage and sheds every
   mob, which holds back a tank or damage dealer but costs a healer
   nothing - Holy keeps its bubble. Hand of Protection stops its wearer
   from attacking, so only a tank, melee or hunter counts it as limiting.
   Divine Intervention always is.
2. **Dropping it is safe** (`safe to drop immunity` value): the bot is
   out of combat; or the fight is over - nothing alive on the group's
   attacker list, no duel, no enemy player in sight - and only the
   core's in-combat flag has yet to run down (it lingers up to 5 s after
   the last mob dies or a duel ends, which is exactly when a solo bot
   used to sit in Ice Block for nothing); or it is above
   `AiPlayerbot.MediumHealth` and nothing would resume attacking it - no
   mob on the attacker list holds more threat on the bot than on its
   current victim (the usual "who is attacking me" reads empty under an
   immunity, since the core makes mobs retarget; the threat list says
   who comes straight back), and no enemy player within
   `AiPlayerbot.SightDistance`. A tank wants the mobs back and only
   needs the health. So a paladin bubbled against three enemy players
   stays bubbled until they are gone, whatever its health, and a solo
   mage whose mob is still waiting for it sits out the block - but drops
   it the moment the mob dies or the duel is called.
   Divine Intervention is the exception: cancelling it mid-fight throws
   away the paladin who died casting it, so only "out of combat" counts.

The strategy only manages an immunity it can account for. The bot's own
casts (Ice Block, Divine Shield, Dispersion) record the trigger that
fired them and when (`immunity cast` value, matched to the aura's apply
time); only a survival trigger - `critical health`, `low health`, `low
mana` - makes the aura eligible. A raid strategy that Ice Blocks to dodge
a mechanic, or a `cast ice block` from the master, leaves no such record
and that immunity runs its course. Divine Intervention and Hand of
Protection are cast on the bot by others and are always managed. Hunter
Deterrence is left alone: five seconds, and the hunter can still move and
trap under it. No config knobs.

## Emote exchanges that end

Bot-to-bot emote exchanges end instead of looping. Upstream bots reacting
to each other's emotes could ping-pong forever: a reply aimed at the other
bot was always answered, and replying left the responder targeting the
emoter, making every further reply aimed. Now a bot replying to *another
bot's* emote (or continuing an emote conversation with one) rolls the new
`AiPlayerbot.EmoteReplyChanceToBots` option (default 30%), so exchanges
trail off naturally after a reply or two. Crowds don't reply in chorus
either: the first bot to reply "claims" the emoter for
`AiPlayerbot.EmoteReplyClaimSeconds` (default 10), silencing the rest of
the crowd toward it — without this, a battleground graveyard or bank full
of bots would answer every emote with a dozen more and the storm would
sustain itself. Replies to real players are unchanged.

## Tunable unprompted emoting

With `AiPlayerbot.RandomBotEmote` on,
upstream bots near other players made a talk gesture roughly every
15 seconds and rolled a random emote on a fixed timer, with no way to
tune it short of disabling emotes entirely. The new
`AiPlayerbot.UnpromptedEmoteChance` option (0-100, default 100 = upstream
frequency) is rolled each time an unprompted emote timer fires, so idle
emoting can be thinned out without touching reactions: replies to
received emotes and emotes commanded by a master are unaffected.

## Tunable duel chatter

Bots say fixed lines around duels: a solicitation while loitering at a
gate duel spot ("Anyone up for a duel?"), sometimes a challenge line as
the flag goes down ("Care for a duel?"), and a winner/loser exchange
afterwards ("Good fight!" / "gg"). `AiPlayerbot.DuelChatter` (default 1)
keeps that behavior; 0 drops the spoken lines while leaving the duels and
their emotes (flex, roar, cheer, salute, …) untouched — for setups where
another module voices these moments instead, as Felworld's llm session
mode does with
[mod-llm's duel talk](https://github.com/felworld/mod-llm/blob/main/FEATURES.md#duel-talk).

## Group hellos and goodbyes that arrive one at a time

Bots say canned lines around group changes: a hello when one logs in, is
invited, or adopts you as its master, "Joining as healer, 2 healer spots
left." when answering `lfg`, a goodbye when it leaves the group or logs
out, and a thanks when a
[quest-competition group](#quest-competition-groups) finishes its camp.
Every one of those events is noticed by the whole group on the same tick,
so upstream produced a wall: add five alts and get five hellos at once,
leave the party and get five goodbyes.

Speaking is now rationed per group. The first bot to reach the event
rolls how many bots will speak at all — `AiPlayerbot.GroupChatterChance`
(default 85) that anybody does, then `AiPlayerbot.GroupChatterFalloff`
(default 30) for each speaker after the first — and the rest of the group
claims slots against that quota until it runs out. The count comes out
geometric: at the defaults, 15% of the time nobody says anything, 60% one
bot does, 18% two, 5% three, and the shape is the same in a party of two
or a raid of twenty-five. Speakers are staggered a second or two apart
(and never answer on the event's own tick), so a second hello reads as an
answer to the first rather than an echo of it.

`AiPlayerbot.GroupChatter = 0` silences the whole family in one place, for
setups where another module voices these moments instead — as Felworld's
llm session mode does with
[mod-llm's group greetings](https://github.com/felworld/mod-llm/blob/main/FEATURES.md#group-greetings).
`AiPlayerbot.EnableGreet` still gates the invite-accept hello on top of
this.

## Faction-honest chat

Bot chat honors the faction wall (`AllowTwoSide.Interaction.Chat`).
Upstream bots said and yelled in the universal language, readable by both
factions, and would whisper or answer chatter across the faction line —
none of which a real player can do. Now bots speak Common/Orcish like
everyone else (universal only when the server config allows cross-faction
chat), refuse to whisper the opposite faction (GMs excepted, matching the
core), and ignore chatter they couldn't understand. Commands were already
faction-safe via the playerbot security layer.

## Runtime bot toggle

`.playerbots enable|disable|status` GM/console commands: flip random bots
on or off at runtime without a restart. Disabling logs out all random bots
and stops repopulation (player-owned alt bots are untouched); enabling
refills the population automatically. This is a runtime override — a config
reload or restart reverts to `AiPlayerbot.Enabled` (which Felworld drives
per session mode via the `AC_AI_PLAYERBOT_ENABLED` env var).

## Ghost perception

A released ghost's client culls the living world: only other ghosts, spirit
healers, gameobjects, grouped teammates, and units near the ghost's own
corpse still render. Bot perception was a raw grid scan that ignored all of
that, so a dead bot kept "seeing" — and reacting to — live players and NPCs
at full sight range. While ghosted, the nearest-unit perception values now
run through the same server visibility check a real client is subject to.
No config knobs.

## Prompt spirit release

Upstream hung the release-spirit decision off a periodic dice roll — one
1-in-5 chance every few seconds — so a dead bot lay there for fifteen
seconds on average, sometimes a minute, before releasing. The decision is
now evaluated every AI tick, with a fixed two-second pause first so the
release still reads as a hand reaching for the button rather than a
reflex. All the deliberate holds are unchanged: battleground release
timing, the soulstone hold above, and a grouped bot near its real-player
leader still waiting for a rez instead of ghost-running.

## Corpse-run pacing

The [core fork](https://github.com/felworld/azerothcore) adds
`Rate.MoveSpeed.Ghost`, a movement speed rate for dead player ghosts
running back to their corpse. `AiPlayerbot.GhostMoveSpeedRate` (default
1.0) multiplies that rate for bot sessions — the same shape as
`AiPlayerbot.RandomBotXPRate` on top of the server XP rate — so bot corpse
runs can be paced separately from human ones, e.g. quick corpse runs for
humans while bots stay dead as long as a normal one takes.

## Resurrection sickness for bots

The [core fork](https://github.com/felworld/azerothcore) adds an
`OnPlayerResurrectSicknessLevel` hook that lets modules adjust the
`Death.SicknessLevel` a player is held to when taking a spirit-healer
resurrection. `AiPlayerbot.ResurrectionSicknessLevel` (default 0 = follow
the server setting) replaces that level for bot sessions.
[Our configs](https://github.com/felworld/configs) disable sickness
server-wide and restore the standard threshold for bots: humans skip the
debuff, while a bot that opts for an instant spirit-healer rez still pays
its cost.

## Deserter debuffs for bots

The [core fork](https://github.com/felworld/azerothcore) adds
`OnPlayerBattlegroundDeserterDebuff` / `OnPlayerDungeonDeserterDebuff`
hooks that let modules decide per player whether leaving a battleground in
progress or an LFG dungeon early casts the Deserter debuff, after the
server's `Battleground.CastDeserter` / `DungeonFinder.CastDeserter` have
been applied. `AiPlayerbot.CastDeserter` (default 0 = follow the server
options) casts it regardless of those options when the leaver is a bot, or
is a real player who leaves another real player behind in the battleground
or group. [Our configs](https://github.com/felworld/configs) disable both
server options and turn this on: a human who bails on an all-bot run skips
the debuff — nobody real was let down — while bots still pay it (and
already refuse to queue while Deserter is up), and a human who abandons
another human pays as usual.

## Unseen stuck recovery

When a travelling bot makes no real progress toward its destination for
90 seconds — usually mmap pathing oscillating around an obstacle — the
RPG movement system gives up walking and teleports it the rest of the
way. That recovery blink is now held back while a real player is within
150 yards (the same guard the world-PvP and duel-spot teleports already
use): the bot keeps walking on a fresh stuck window and only blinks once
nobody is watching. The stuck clock also restarts after any interruption
that pauses travel (combat, dying, the corpse run), so a bot delayed by
a fight no longer vanishes mid-stride the moment it gets moving again —
previously the timer kept aging through the whole ordeal and fired the
teleport in plain view.

## Class service commands

The class utilities other players provide on a busy server — a mage handing
out food and water, a mage opening a city portal, warlocks summoning a group
member — issued as explicit commands to your bots:

- `!conjure food` / `!conjure water` (mage) — the bot casts its best
  Conjure Food/Water (Conjure Refreshment at high level, which covers
  both requests) and hands the conjured stacks straight to you, walking
  over to deliver them if you're not beside it (within visual range —
  it won't chase you across the zone). Anything already conjured in its
  bags is handed over immediately without a cast.
- `!portal <city>` (mage) — the bot casts the matching Portal spell
  (`!portal stormwind`, `!portal shattrath`, …); step through before it
  fades. `!portal` alone lists the destinations it knows — it can only
  open portals its faction and level have learned. Anyone can whisper it
  to a world bot: strangers get quoted a tip instead of a free cast (see
  below).
- `!ritual` (warlock) — summons *you* with a real Ritual of Summoning:
  the warlock begins the channel and two group bots standing with it
  click the summoning portal, so wherever you are you get the standard
  summon-accept dialog — the classic warlock taxi for skipping the walk
  back to your party. The game only lets group members work the portal,
  but you don't have to arrange the group yourself: whisper `!ritual` to
  any warlock world bot and, if you're not already grouped with it, *it*
  invites *you* — accept the invite and the ritual begins. (The invite
  has to run in that direction: inviting a distant world bot into your
  own group teleports it to you on accept, which defeats the summon.
  The bot stops waiting after a minute if you don't accept.) The two
  portal clickers don't have to be groupmates either: if the group
  doesn't have two members standing with the warlock, it recruits nearby
  ungrouped world bots into the group for the ritual, like asking
  strangers at a summoning stone — they click, then quietly leave the
  group a minute later. (An internal `use summoning portal` command also
  makes group bots click a ritual *you* cast as a warlock.) Strangers
  are quoted a tip, payable after they land (see below).

Portals and summons are favors inside the bot's own circle — its master,
group, and guild ride free — and a paid service for everyone else, the
way players tip for them on a busy server. A stranger's `!portal <city>`
gets a quoted tip (`AiPlayerbot.ClassService.PortalTip`, default 50s,
jittered ±20%) collected through a real trade window before the cast —
and the mage travels over first, the same simulated ride as a
[cross-city trade deal](#city-market-trading), when the customer is in
another city. A stranger's `!ritual` quotes the summon tip
(`AiPlayerbot.ClassService.SummonTip`, default 1g) and runs the ritual
first — payment can't precede a summon, since the customer is far away by
definition — then the warlock collects by trade once they land; skipping
out on the bill just means the deal expires (and only world bots sell,
never someone's alt). Setting a tip to 0 keeps that service circle-only,
refusing strangers. Service deals share the one-deal-per-bot slot, the
market anchor, and the fulfillment machinery of
[city market trading](#city-market-trading).

Reagents are the one concession to convenience: a bot missing the Rune of
Portals or Soul Shard a spell consumes produces one in its bags first —
bots don't shop for reagents. Everything else uses the real spells, cast
times, and mechanics, so it all plays out visibly in the world. These
commands are also the hooks mod-llm's LLM-driven bots answer
natural-language requests with — a plain "can someone summon me?" to an LLM
bot triggers the same machinery; see
[mod-llm's class services](https://github.com/felworld/mod-llm/blob/main/FEATURES.md#class-services).

## Observability metrics

When the core's `Metric.Enable` is on (the Felworld obs stack — see the
[hub FEATURES.md](https://github.com/felworld/azerothcore/blob/main/FEATURES.md#observability)),
this fork feeds the Grafana dashboards; with metrics off, every emission is
a no-op:

- **Census** — the 300-second bot census that already logs to
  `Playerbots.log` is mirrored to the metrics bus: bots online, level
  histogram by faction, per-race/class counts, role split, activity and
  engine states, RPG statuses, cumulative quest throughput, per-zone
  population, and total gold held per faction (`playerbots_gold`, in
  copper).
- **Lifecycle** — every death counts a `playerbots_deaths` point tagged
  bot/player, faction, zone, and context (world vs. battleground vs.
  arena, so BG mayhem doesn't drown the world-danger signal), and every
  level gained counts `playerbots_levelups`; both also land on the
  character's `felworld_events` timeline (`death` with zone and level,
  `level_up` with from/to). Battleground rounds count `playerbots_bg`
  starts and ends, ends tagged with the winning faction.
- **Chat** — every outbound bot message counts one `playerbots_chat` point
  tagged with where it went (say, yell, whisper, party, raid, guild, or a
  public channel); broadcast attempts whose channel rolls all failed count
  `playerbots_broadcast_suppressed`, so the roll economy is visible.
- **World PvP** — defense-board events (`kill`, `attacker_death`,
  `callout`, `defender_on_scene`, `escalation`, plus `went_home` when an
  ended excursion teleports an outleveled bot out of the zone) and
  excursion starts (by origin: own roll vs. defense response) and ends
  (by reason). Kills,
  deaths, defeats, callouts, and excursion boundaries are also written to
  the characters DB `felworld_events` table per participant, which is what
  the Character Inspector dashboard replays.

## Commands added in this fork

Bot chat commands — spoken to a bot like any command from the
[upstream command list](https://github.com/mod-playerbots/mod-playerbots/wiki/Playerbot-Commands),
with the `!` prefix described above:

- `!grind quests` — switch the bot to the quest-aware grind strategy
  (replaces plain `grind` and vice versa; `!nc -grind quests` turns it
  off).
- `!bg strategy <attack fc|attack base|defend fc|defend base>` — call a
  play in Warsong Gulch battleground chat; bots on your team each follow
  the order with configurable probability for a limited time, announcing
  what they're doing. Also the hook behind mod-llm's `bg_strategy` tool,
  so in LLM mode plain callouts like "inc!!" work too. Detailed in
  [Warsong Gulch teamwork](#warsong-gulch-teamwork).
- `!wpvp defend [zone]` — order a random bot to travel in and defend a
  zone; with no argument, the zone *you* are standing in. Subzone names
  work too ("Tarren Mill" finds Hillsbrad Foothills). The bot heads for
  the last reported attacker position there if defenders have called one
  in, else to your side when you're calling from inside that zone,
  otherwise for the zone's own gathering spot, and whispers back an "On
  my way" (or a refusal if it doesn't recognize the zone). Any player can
  issue it, but only world (random) bots accept — your own alt bots ignore
  it. This is also the hook behind mod-llm's `go_defend` tool, so in LLM
  mode you can simply ask a bot in plain language to go help.
- `!conjure food` / `!conjure water`, `!portal <city>`, `!ritual` — the
  mage and warlock class services described in
  [Class service commands](#class-service-commands).
- `!wts <itemlink> [count] [price]` / `!wtb <itemlink> [count] [price]` —
  sell to or buy from a bot for real; with a sane concrete price the bot
  commits, walks over, and completes the trade. `!appraise <itemlink>` and
  `!sellables` query what a bot wants, has, and would pay or ask;
  `!sellto` / `!buyfrom <player> <itemlink> [count] <price>` commit a
  negotiated deal (master/LLM only). Detailed in
  [City market trading](#city-market-trading).

GM / console commands:

- `.playerbots enable | disable | status` — the runtime random-bot toggle
  described above (administrator).
- `.playerbots wpvp test [class]` — send an opposing-faction bot on a
  world-PvP excursion to your position, optionally filtered by class
  (in-game GM only).
- `.playerbots wpvp off [minutes] | on` — excursion kill switch: `off`
  disables new excursions for the given minutes (no argument uses
  `AiPlayerbot.WpvpKillSwitchDefaultMinutes`; `0` means until restart) and
  immediately sends every bot currently out on one home; `on` re-enables
  early.
- `.playerbots wpvp status` — whether excursions are enabled, plus one line
  per bot currently out: destination, travelling or dwelling, and deaths so
  far.
- `.playerbots gear [player] <white|green|blue|epic|legendary> [max item level]`
  — non-destructively re-gear an online character (targeted, named, or
  yourself) with factory-picked items of the given rarity, using the same
  spec-aware stat weighting that outfits bots: every slot is re-rolled, the
  new gear is enchanted and gemmed (at the `AiPlayerbot.MinEnchantingBotLevel`
  threshold), ammo is restocked, and durability repaired. Replaced items are
  moved to the character's bags, never destroyed — a slot whose old item
  doesn't fit in the bags is left unchanged, and the command warns up front
  when bag space looks short. The optional trailing number caps the item
  level. The rarity is exact: unlike the bot population (which rolls
  `AiPlayerbot.RandomGearLoweringChance` and pads thin item pools with lower
  tiers), the command never equips below the requested tier — a slot with no
  candidates at that tier keeps its current gear. The factory's low-level
  slot gates still apply: no rings below level 20, no helmet/neck below 30,
  no trinkets below 50. Made for GM-leveled test characters
  (`.character level` and go), and works on bots too (gamemaster; works from
  the console with an explicit player name).

---

Felworld is a non-commercial research project. It contains no game client,
assets, or proprietary code, and is not affiliated with or endorsed by
Blizzard Entertainment — see the
[project disclaimer](https://github.com/felworld/azerothcore#license-and-disclaimer).
