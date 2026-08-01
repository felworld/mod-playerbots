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
drifts back to whatever it was doing — and repeating the same call within
that window doesn't re-roll the dice, so spamming can't whip the whole
team into line. Flag carriers ignore orders and keep running the flag,
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

## Social buffing

Idle ungrouped mages, priests, druids, and paladins cast
their signature class buff (Arcane Intellect, Fortitude, Mark of the Wild,
a fitting Blessing) on nearby friendly players — real players and bots —
who lack it, and buff-capable bots return the favor when someone buffs
them. Bots also answer a stranger's heal with a targeted /thank emote
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

## Quest-competition groups

A solo random bot that sees a nearby ungrouped
same-faction real player — within 4 levels — fighting a
creature the bot still needs for an in-progress quest silently invites
them to a group, the way real players resolve spawn competition. While
grouped the bot keeps grinding as a peer rather than trailing its new
partner: it targets whatever mobs anyone in the group still needs, so
the pair naturally splits a camp and shares kill credit. Mob types the
bot fights that a member still needs extend the group's shared
objectives, so migrating to the next camp together keeps the group
alive — but unrelated shared quests two zones away never hold it
together. Once nobody in the group needs any of those mobs, the bot
says thanks in party chat and leaves. Declined invites go on a
per-player cooldown so nobody gets pestered.
`AiPlayerbot.QuestCompetitionInvite` (default on) and
`AiPlayerbot.QuestCompetitionInviteCooldown` in `playerbots.conf.dist`.

## Chest roll-offs

In a group that contains a real player, bots don't ninja-loot world
chests anymore: when the group spots a lootable chest, every bot that
could open it does a visible `/roll` (1–100) and only the highest
roller walks over and opens it. The real player can enter the contest
too by typing `/roll` within the ~5-second window — and if they win,
the bots leave the chest alone (bots re-consider it after 60 seconds if
the winner never collects, so a passed-up chest doesn't stay locked
forever). Only genuinely contested chests get a roll-off: gathering
nodes, quest chests, and chests with group loot rules (whose contents
already go through real need/greed rolls) are exempt, as are all-bot
groups, which loot the way they always did.
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

## Busy capital cities

Bots that find themselves in a friendly (own-faction or neutral) capital
mostly stay a while, wandering between the bank, auction house, inn, and
vendor NPCs or sitting down to rest, instead of immediately rolling their
next activity somewhere out in the world. Combined with upstream's
periodic teleport-to-a-city-banker (`AiPlayerbot.ProbTeleToBankers`),
this keeps a standing crowd around the Orgrimmar and Stormwind bank/AH/inn
districts, the way capitals feel on a busy server — while bots still
eventually leave, so the faces turn over. `AiPlayerbot.CityDwellChance`
(default 0.8) is the per-reroll probability of staying; bots re-roll about
every 5 minutes, so the mean visit is `5min / (1 - chance)` (~25 minutes
at the default). Set to 0 to disable.

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
Shadowmeld and hold the ambush instead.
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
defending side (a friendly player, their pet, or the town's guards and
civilians), or a still-fresh ganker the defense channels already named
merely prowling past — and the wording follows what was seen ("Bloguk is
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
squash — and once such a defender is on the scene,
the escalation is held entirely: with help visibly arrived, even the
victims stop pleading, until that defender dies or leaves.
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
`AiPlayerbot.WpvpDefense*` and `AiPlayerbot.WpvpReinforcement*` knobs;
the `!wpvp defend` chat command ([below](#commands-added-in-this-fork))
lets you order a defense yourself.

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
  self-res while a PvP-flagged enemy is within 40 yards, waiting up to a
  minute for them to leave before giving up on it and releasing normally.
- **No rezzing into campers**: a dead bot ran back and popped up at its
  corpse — or took the spirit-healer res — regardless of who was standing
  on it. Ghosts now wait out a flagged enemy loitering at the rez spot,
  and only after about three minutes of being camped give up and rez
  anyway.
- **No dueling next to a battlefield**: idle bots would happily start a
  recreational duel while a gank unfolded across the road. Bots now
  neither offer nor accept another bot's duel while world PvP is live
  nearby — a PvP kill or defense callout in their zone within the last
  two minutes, or a PvP-flagged enemy player inside vision range
  (`AiPlayerbot.WpvpVisionDistance`). A real player's challenge is still
  accepted; a human read the room.

No config knobs; the thresholds (40 yards, one minute, two minutes, three
minutes) are fixed.

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
hurting, it drops a target dummy to shed aggro; now and then it releases
an explosive sheep. Glove tinkers fire on cooldown. Rocket boots pop
when it matters most: carrying a Warsong Gulch flag, chasing an enemy
flag carrier who is pulling away, or fleeing below 25% health. And after
the fight, a bot with no real resurrection spell but a set of jumper
cables will walk to a dead group member and try a jump-start — the
item's native fail chance supplies the comedy. Duels honor
`AiPlayerbot.DuelConsumables` (engineering items are tier `1`, the
Felworld setting). No other config knobs.

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
  open portals its faction and level have learned.
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
  makes group bots click a ritual *you* cast as a warlock.)

Reagents are the one concession to convenience: a bot missing the Rune of
Portals or Soul Shard a spell consumes produces one in its bags first —
bots don't shop for reagents. Everything else uses the real spells, cast
times, and mechanics, so it all plays out visibly in the world. These
commands are also the hooks mod-llm's LLM-driven bots answer
natural-language requests with — a plain "can someone summon me?" to an LLM
bot triggers the same machinery; see
[mod-llm's class services](https://github.com/felworld/mod-llm/blob/main/FEATURES.md#class-services).

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
  zone; with no argument, the zone *you* are standing in. The bot heads for
  the last reported attacker position there if defenders have called one
  in, otherwise for the zone's own gathering spot, and whispers back an "On
  my way" (or a refusal if it doesn't recognize the zone). Any player can
  issue it, but only world (random) bots accept — your own alt bots ignore
  it. This is also the hook behind mod-llm's `go_defend` tool, so in LLM
  mode you can simply ask a bot in plain language to go help.
- `!conjure food` / `!conjure water`, `!portal <city>`, `!ritual` — the
  mage and warlock class services described in
  [Class service commands](#class-service-commands).

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
