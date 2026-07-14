# mod-playerbots (Felworld fork)

Part of [Felworld](https://github.com/felworld/azerothcore) — an AI-populated,
(mostly) single-player WoW 3.3.5a world. This is Felworld's fork of
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
  like a questing partner instead of slaughtering everything in sight. Say
  `!grind quests` to the bot in chat to enable it (`grind` and `grind quests`
  replace each other; `!nc -grind quests` turns it off).
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
