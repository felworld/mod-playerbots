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
