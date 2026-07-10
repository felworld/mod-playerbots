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
  prefix are never parsed as commands — they're ordinary chat, which (with
  Ollama enabled) goes to mod-ollama-chat instead of being silently eaten
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
  stationary carrier.
- `.playerbots enable|disable|status` GM/console commands: flip random bots
  on or off at runtime without a restart. Disabling logs out all random bots
  and stops repopulation (player-owned alt bots are untouched); enabling
  refills the population automatically. This is a runtime override — a config
  reload or restart reverts to `AiPlayerbot.Enabled` (which Felworld drives
  per session mode via the `AC_AI_PLAYERBOT_ENABLED` env var).

There is no separate install procedure: the module is a submodule of
felworld/azerothcore and builds into the containerized server there. Our
playtested `playerbots.conf` lives in
[felworld/configs](https://github.com/felworld/configs).

Upstream documentation (behavior, commands, strategies) is in the
[mod-playerbots wiki](https://github.com/mod-playerbots/mod-playerbots/wiki).
