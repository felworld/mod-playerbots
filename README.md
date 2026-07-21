# mod-playerbots (Felworld fork)

Part of [Felworld](https://github.com/felworld/azerothcore) — a tech demo of
AI "players" (LLM agents + classical game AI) populating and interacting in
an MMO world. This is Felworld's fork of
[mod-playerbots/mod-playerbots](https://github.com/mod-playerbots/mod-playerbots),
the AzerothCore module that fills the world with bot "players" that level,
quest, group, run dungeons, and fight in battlegrounds. It does what it says
on the tin; our changes are quality-of-life on top.

We track upstream and merge in improvements periodically. Note the version
coupling: this module requires the matching
[playerbots fork of AzerothCore](https://github.com/mod-playerbots/azerothcore-wotlk),
which is what [felworld/azerothcore](https://github.com/felworld/azerothcore)
is forked from — the two move together.

## Felworld changes

- Bot commands require a `!` prefix. Felworld sets upstream's
  `AiPlayerbot.CommandPrefix` option to `!`, so every command from the
  [playerbot command list](https://github.com/mod-playerbots/mod-playerbots/wiki/Playerbot-Commands)
  is written `!follow`, `!attack`, `!who warrior`, etc. Messages without the
  prefix are never parsed as commands — they're ordinary chat, which (in LLM
  mode) goes to mod-llm instead of being silently eaten
  because it happened to start with a command word ("who said that?"). Our
  fork also fixes the bot's internally re-queued commands (repeated `cast`)
  to respect the prefix, which upstream's option didn't.
- `grind quests` strategy: like upstream's `grind` ("attack any visible
  target"), but the bot only engages mobs that someone in its group still
  needs for an incomplete quest — kill credit or a quest-item drop —
  including neutral and gray ones. The result is a bot that pulls and fights
  like a questing partner instead of slaughtering everything in sight.
  Toggled with the `!grind quests` chat command (below).
- Warsong Gulch bots play the objective like a team. Upstream's
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
- Bystander assist: solo random bots rescue nearby non-group players — real
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
- Social buffing: idle ungrouped mages, priests, druids, and paladins cast
  their signature class buff (Arcane Intellect, Fortitude, Mark of the Wild,
  a fitting Blessing) on nearby friendly players — real players and bots —
  who lack it, and buff-capable bots return the favor when someone buffs
  them. Bots also answer a stranger's heal with a targeted /thank emote
  (mod-llm supplies the spoken "thx"). Like bystander assist, no action is
  ever taken that would newly PvP-flag the bot: flagged targets are only
  buffed by already-flagged bots. `AiPlayerbot.EnableSocialBuffing` and
  `AiPlayerbot.EnableHealThanks` (both default on) plus radius/cooldown
  knobs in `playerbots.conf.dist`.
- Quest-competition groups: a solo random bot that sees a nearby ungrouped
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
- World PvP excursions: random bots occasionally travel to enemy or contested
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
  commands (listed below) provide a test hook, observability, and a runtime
  kill switch.
- World PvP defense and reinforcements: ganking has consequences, on both
  sides. Defender bots call out invaders in LocalDefense ("Bloguk is
  attacking Goldshire!"), throttled per zone and per attacker, and an
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
  the `!wpvp defend` chat command (below) lets you order a defense
  yourself.
- Bot-to-bot emote exchanges end instead of looping. Upstream bots reacting
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
- Unprompted emoting is tunable. With `AiPlayerbot.RandomBotEmote` on,
  upstream bots near other players made a talk gesture roughly every
  15 seconds and rolled a random emote on a fixed timer, with no way to
  tune it short of disabling emotes entirely. The new
  `AiPlayerbot.UnpromptedEmoteChance` option (0-100, default 100 = upstream
  frequency) is rolled each time an unprompted emote timer fires, so idle
  emoting can be thinned out without touching reactions: replies to
  received emotes and emotes commanded by a master are unaffected.
- Bot chat honors the faction wall (`AllowTwoSide.Interaction.Chat`).
  Upstream bots said and yelled in the universal language, readable by both
  factions, and would whisper or answer chatter across the faction line —
  none of which a real player can do. Now bots speak Common/Orcish like
  everyone else (universal only when the server config allows cross-faction
  chat), refuse to whisper the opposite faction (GMs excepted, matching the
  core), and ignore chatter they couldn't understand. Commands were already
  faction-safe via the playerbot security layer.
- `.playerbots enable|disable|status` GM/console commands: flip random bots
  on or off at runtime without a restart. Disabling logs out all random bots
  and stops repopulation (player-owned alt bots are untouched); enabling
  refills the population automatically. This is a runtime override — a config
  reload or restart reverts to `AiPlayerbot.Enabled` (which Felworld drives
  per session mode via the `AC_AI_PLAYERBOT_ENABLED` env var).

## Commands added in this fork

Bot chat commands — spoken to a bot like any command from the
[upstream command list](https://github.com/mod-playerbots/mod-playerbots/wiki/Playerbot-Commands),
with the `!` prefix described above:

- `!grind quests` — switch the bot to the quest-aware grind strategy
  (replaces plain `grind` and vice versa; `!nc -grind quests` turns it
  off).
- `!wpvp defend [zone]` — order a random bot to travel in and defend a
  zone; with no argument, the zone *you* are standing in. The bot heads for
  the last reported attacker position there if defenders have called one
  in, otherwise for the zone's own gathering spot, and whispers back an "On
  my way" (or a refusal if it doesn't recognize the zone). Any player can
  issue it, but only world (random) bots accept — your own alt bots ignore
  it. This is also the hook behind mod-llm's `go_defend` tool, so in LLM
  mode you can simply ask a bot in plain language to go help.

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

## Tests

`test/` holds the module's Google Test sources (the `!` command prefix, the
`.playerbots` runtime toggle, the bystander-assist distress predicate),
registered with the core test target by
`mod-playerbots.cmake` and built/run through the core repo's
`apps/docker/run-unit-tests.sh`.

There is no separate install procedure: the module is a submodule of
felworld/azerothcore and builds into the containerized server there. Our
playtested `playerbots.conf` lives in
[felworld/configs](https://github.com/felworld/configs).

Upstream documentation (behavior, commands, strategies) is in the
[mod-playerbots wiki](https://github.com/mod-playerbots/mod-playerbots/wiki).

---

Felworld is a non-commercial research project. It contains no game client,
assets, or proprietary code, and is not affiliated with or endorsed by
Blizzard Entertainment — see the
[project disclaimer](https://github.com/felworld/azerothcore#license-and-disclaimer).
