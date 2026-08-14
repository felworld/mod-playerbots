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

The full tour — with the config options and rationale behind each behavior —
is in [FEATURES.md](FEATURES.md). In brief:

- **[Command prefix](FEATURES.md#command-prefix)** — bot commands require a
  `!` prefix (`!follow`, `!attack`, `!who warrior`); unprefixed messages are
  ordinary chat, never silently swallowed as commands.
- **[Quest-aware grinding](FEATURES.md#quest-aware-grinding)** — `!grind
  quests` makes a bot pull only mobs its group still needs, like a questing
  partner.
- **[Runner focus fire](FEATURES.md#runner-focus-fire)** — DPS bots burn
  down a dungeon mob fleeing for reinforcements before the adds arrive,
  without ever overriding a skull mark or breaking fear CC.
- **[Human reaction latency](FEATURES.md#human-reaction-latency)** —
  interrupts, dispels, and emergency heals fire after a jittered
  human-scale delay, and occasionally miss, instead of triggering
  instantly every time.
- **[Pet group etiquette](FEATURES.md#pet-group-etiquette)** — bot pets
  assist their owner's target, keep taunts off in instanced groups, and
  heel on disengage instead of body-pulling the next pack.
- **[Dependable LFD port-in](FEATURES.md#dependable-lfd-port-in)** —
  bots stranded outside their Dungeon Finder instance retry the teleport
  until they land, and a mid-fight bot defers a group-ready proposal
  instead of declining it for everyone.
- **[Warsong Gulch teamwork](FEATURES.md#warsong-gulch-teamwork)** —
  flag-carrier escorts and peels, flag-room defenders, roles re-decided on
  death, stealthy approaches, incoming callouts in BG chat, and a
  `bg strategy` command for calling plays your teammates actually follow.
- **[Staggered battleground mounts](FEATURES.md#staggered-battleground-mounts)**
  — bots mount one by one during the prep phase instead of the whole team
  snapping onto mounts the instant the timer hits 30 seconds.
- **[Bystander assist](FEATURES.md#bystander-assist)** — solo bots rescue
  nearby non-group players who look about to die, when the fight looks
  winnable; healer classes also support strangers losing a PvP fight and
  keep the heals coming once committed.
- **[Social buffing](FEATURES.md#social-buffing)** — idle bots buff
  passers-by, return buffs, and thank strangers for heals.
- **[Stealth-spotting reactions](FEATURES.md#stealth-spotting-reactions)** —
  a bot that detects someone stealthed nearby freezes and snaps around to
  face them, sometimes waving at a friendly sneak or calling out a hostile
  one.
- **[Stealth flushing](FEATURES.md#stealth-flushing)** — an enemy who
  vanishes on a bot gets their last-known spot swept: Consecration, Flare
  and traps, AoE pulses, and Faerie Fire on a revealed rogue so they can't
  restealth.
- **[Quest-competition groups](FEATURES.md#quest-competition-groups)** — a
  bot competing with you for spawns invites you to group and grinds
  alongside you as a peer.
- **[Chest roll-offs](FEATURES.md#chest-roll-offs)** — grouped bots `/roll`
  for world chests instead of ninja-looting them, and you can roll too;
  the highest roller opens it.
- **[Roll-win giveaways](FEATURES.md#roll-win-giveaways)** — a bot that
  wins a roll on gear it can't use sometimes trades it to the group
  member who actually needs it.
- **[City market trading](FEATURES.md#city-market-trading)** — bots buy
  and sell with players for real: `!wts`/`!wtb` offers get appraised at
  market-ish prices, and a committed deal ends with the bot coming over —
  cross-city if need be — and trading items for gold through an actual
  trade window.
- **[Busy capital cities](FEATURES.md#busy-capital-cities)** — bots in a
  friendly capital linger around the bank/AH/inn district instead of
  immediately heading back out, keeping Orgrimmar and Stormwind populated.
- **[World PvP excursions](FEATURES.md#world-pvp-excursions)** — bots
  travel to enemy or contested towns to lurk and pick fights, with goading
  emotes and level-gap-curved gankers.
- **[World PvP defense and reinforcements](FEATURES.md#world-pvp-defense-and-reinforcements)**
  — gank sprees draw LocalDefense/WorldDefense callouts, defenders travel
  in to hunt the attacker, a beaten ganker can pull reinforcements, and
  emoting at an enemy rallies every bot that saw you do it — it all works
  the same when the ganker is a real player. WorldDefense itself becomes
  a normal opt-in channel: `/join WorldDefense` works like any custom
  channel.
- **[Guard respect](FEATURES.md#guard-respect)** — bots don't pick or
  chase world-PvP fights under the cover of hostile guards that far
  outlevel them; a fleeing enemy who reaches their guards gets let go.
- **[Same-class truce](FEATURES.md#same-class-truce)** — some same-class
  pairs honor the "druids don't gank druids" code: no unprovoked attack,
  a `/salute` instead.
- **[Terrain line of sight](FEATURES.md#terrain-line-of-sight)** — server
  LOS sees through hills (only model geometry is ray-tested), so bots
  additionally check the terrain heightmap before noticing an enemy
  player; no more being spotted from behind a ridge.
- **[Level perception](FEATURES.md#level-perception)** — bots no longer
  know the exact level of a hostile unit their owner would see as "??"
  (10+ levels above them); callouts say "??" like a player would.
- **[World PvP threat reactions](FEATURES.md#world-pvp-threat-reactions)**
  — attacked in the open world, bots get up from meals, abort long casts,
  turn from mobs to their assailant, hold a soulstone res while the
  killer lurks, wait out corpse campers instead of rezzing into them, and
  put recreational duels on hold while a fight rages nearby.
- **[Corpse-camping satiation](FEATURES.md#corpse-camping-satiation)** —
  after each kill a bot may decide its victim is dealt with and stop
  re-engaging them for a while; most move on after a kill or two, a rare
  few camp on.
- **[Grudges and mercy](FEATURES.md#grudges-and-mercy)** — a killed bot
  remembers its killer: some come back for revenge on sight, the rest
  keep well away, pleading (`/shoo`, `/beg`, `/cry`) and retreating when
  the killer comes near — and begging sometimes genuinely moves a bot
  attacker to break off, for real players too.
- **[Chase break-off](FEATURES.md#chase-break-off)** — a bot whose
  world-PvP target cleanly escapes rolls to give up the chase every so
  often instead of pursuing forever; most turn back quickly, the odd one
  stays dogged, and a runner who comes back is fair game again.
- **[Target peeling](FEATURES.md#target-peeling)** — a bot fighting one
  enemy player switches to another who shows up substantially closer,
  instead of tunneling on its first pick past fresh threats.
- **[Initiation readiness](FEATURES.md#initiation-readiness)** — a bot
  below its health/mana comfort bars passes on starting an open-world
  fight and drinks up first, unless the target's state makes striking
  now the better play.
- **[Passerby assist](FEATURES.md#passerby-assist)** — flagged bots join
  a faction-mate's fight instead of walking past it, and an underleveled
  helper who piles into an even fight gets focused down like an add.
- **[Duel openers](FEATURES.md#duel-openers)** — bots use the pre-duel
  countdown like a player would: rogues and ferals stealth and slip out
  of detection range, warriors back off to Charge range, hunters trap
  their starting spot and step out, casters make room for an opening
  cast.
- **[Stealth flanking](FEATURES.md#stealth-flanking)** — a stealthed bot
  closing on a player circles outside its target's detection ring and
  opens from behind instead of walking the shimmer straight into their
  face — in duels, battlegrounds, and world PvP alike.
- **[The Distract trick](FEATURES.md#the-distract-trick)** — some rogue
  bots (default 60%, rolled per character) skip the circling: they cast
  Distract past a watching target to turn its back, then walk straight
  in for the opener — picking their moments by energy, target danger,
  and whether area damage or detection is already down. And it can
  backfire: a Distracted bot sometimes reads the forced turn for what
  it is, spins around after a human-scale beat, and sweeps the lane
  behind it.
- **[Bandage crafting](FEATURES.md#bandage-crafting)** — idle bots craft
  bandages from the cloth they carry, keep the stock level-appropriate,
  and use their best bandage first.
- **[Engineering in combat](FEATURES.md#engineering-in-combat)** —
  engineer bots throw bombs and grenades, interrupt casters with stun
  grenades, sapper when surrounded, drop target dummies, pop rocket
  boots on flag runs, and jump-start dead groupmates with jumper cables.
- **[Emote exchanges that end](FEATURES.md#emote-exchanges-that-end)** —
  bot-to-bot emote replies trail off instead of ping-ponging forever, and
  crowds don't reply in chorus.
- **[Tunable unprompted emoting](FEATURES.md#tunable-unprompted-emoting)** —
  thin out idle emoting without touching reactions.
- **[Class service commands](FEATURES.md#class-service-commands)** —
  `!conjure food`/`!conjure water`, `!portal <city>`, `!ritual`: mage
  food and water walked over and handed to you, mage city portals, and
  a real warlock summoning ritual — whisper any warlock world bot and it
  invites you to its group for the summon, recruiting bystander bots as
  portal clickers when needed. Portals and summons are free for the
  bot's circle and sold to strangers for a configurable tip, collected
  through a real trade window.
- **[Faction-honest chat](FEATURES.md#faction-honest-chat)** — bots speak
  their faction's language, honor `AllowTwoSide.Interaction.Chat`, and
  ignore speech they couldn't understand.
- **[Runtime bot toggle](FEATURES.md#runtime-bot-toggle)** — `.playerbots
  enable|disable|status` flips random bots live, without a restart.
- **[Corpse-run pacing](FEATURES.md#corpse-run-pacing)** —
  `AiPlayerbot.GhostMoveSpeedRate` scales the core's `Rate.MoveSpeed.Ghost`
  for bots, so their corpse runs can be paced separately from humans'.
- **[Resurrection sickness for bots](FEATURES.md#resurrection-sickness-for-bots)**
  — `AiPlayerbot.ResurrectionSicknessLevel` overrides `Death.SicknessLevel`
  for bots, so they keep paying for spirit-healer rezzes when humans don't.
- **[Observability metrics](FEATURES.md#observability-metrics)** — the bot
  census, chat destinations, and WPvP lifecycle feed the Felworld Grafana
  dashboards when the core's metrics are enabled.

### Commands added in this fork

Spoken to a bot (with the `!` prefix, like every
[upstream command](https://github.com/mod-playerbots/mod-playerbots/wiki/Playerbot-Commands)):

- `!grind quests` — switch to the quest-aware grind strategy.
- `!wpvp defend [zone]` — order a random bot to come defend a zone (the one
  you're standing in if omitted).
- `!conjure food` / `!conjure water`, `!portal <city>`, `!ritual` — mage
  and warlock class services (conjured goods handed to you, city portals,
  summoning rituals).
- `!wts` / `!wtb <itemlink> [count] [price]`, `!appraise <itemlink>`,
  `!sellables`, `!sellto` / `!buyfrom <player> <itemlink> [count] <price>`
  — real buying and selling with bots, fulfilled through actual trade
  windows.

GM / console:

- `.playerbots enable | disable | status` — the runtime random-bot toggle.
- `.playerbots wpvp test [class] | off [minutes] | on | status` — world-PvP
  excursion test hook, kill switch, and status report.
- `.playerbots gear [player] <quality> [max item level]` — non-destructively
  re-gear a character with spec-appropriate items of the given rarity, the
  way the bot factory outfits bots.

Arguments and behavior details:
[FEATURES.md](FEATURES.md#commands-added-in-this-fork).

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
