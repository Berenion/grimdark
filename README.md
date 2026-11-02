# Grimdark Controller

A screen-reader accessibility mod for Grim Dawn.

This is a fork of [Grimdark](https://github.com/ahicks92/grimdark) that adds Xbox and PlayStation controller support
(see Controller below). Everything else is the original mod, merged in as it is updated; the text below is the original
author's. It installs as "Grimdark Controller", next to the original if you have that too, and both share the same
settings.

See [CHANGES.md](CHANGES.md) for what's new in each release.

Status: pre-release, but pretty complete.  I'm into act 3 and even have basic support for hazardous terrain obstacles.

## Disclaimer

This is a hobby project, and it is a mod of a closed-source C++ game.  As of 2026-09-18 I have read 0 lines of code;
it's all Claude. Any game update could break it forever. It is a low priority for me, so bugs are not going to get fixed
promptly; you will have to wait until whenever I have time.  This said, it works well and I've got like 50+ hours in the
game, and it's better than most accessibility mods.  Just know what you're getting into.

For those familiar with my prior work, do not think of this as a Factorio Access. I am not treating it like that. You
get what you get; hopefully you have fun, but it might also explode on you in ways no one can fix.

## What you need

- Grim Dawn **v1.3.0.8, 64-bit, Steam build**. The mod reaches into the game's private UI objects by code layout; any
  other build (a GOG build, a Steam patch) gets one spoken line -- "unsupported game build, exe timestamp <hex>,
  supported 1.3.0.8 Steam, the mod is off" -- and the mod installs nothing. Report that timestamp when you ask about a
  new build. Any install path works; the launcher asks Steam where the game is (see Setup).
- Windows 10/11 x64.
- A screen reader (NVDA, JAWS, or any that prism supports). Menu and window text goes to the screen reader.
- The Windows OneCore voices "Mark" and "Zira" (Settings -> Time & Language -> Speech -> Manage voices, English (United
  States)). These carry the in-world speech: Mark speaks at the enemy's position, Zira is the player and the room
  announcer. If they are missing the default OneCore voice is used for both.
- Steam running (the launcher starts the game outside Steam's Play button, but the game still talks to Steam).
- Visual Studio 2022 only if you build it yourself (see Building, at the end); the CI zip needs nothing else.

## Setup

The mod supports the base game and the full install with both expansions (Ashes of Malmouth and Forgotten Gods). Each
expansion replaces the whole world map, so the mod ships a rooms database for each world and picks the right one from
what is installed. An install with Ashes of Malmouth alone uses the base game's rooms and will be wrong in a few places,
and additionally will not describe rooms in that DLC alone unless you also install Forgotten Gods.  I may eventually
require some or all of the DLC, and do not intend to continue improving anything but the Malmouth/FG setup because
testing all possible combinations is too time consuming.  Fangs of Asterkarn and the Crucible do not yet work.

Do not launch the game directly (from Steam or the exe). If you do, you will need to restart your screen reader because
the key hooks will be dropped.  This manifests as capslock/insert not working as they should.  The game uses very old
APIs in very odd ways, the net effect of which is to break the JAWS / NVDA key. The mod fixes this, but only when it is
injected at launch by our launcher.  If you make this mistake, close the game, restart your screen reader, and go
through the launcher.

Download `GrimdarkControllerInstaller.exe` from the newest release on
[this fork's releases page](https://github.com/Berenion/grimdark/releases) and run it. Install lists the released versions
newest first and, last, "latest successful CI build", which is the untested build of the current source and is there for
people who know they want it.  You get updates by running this same installer again.  The mod does not currently check
for updates itself.

You now have a desktop icon `Grimdark Controller`.  You launch the game through this icon instead of the game's own.  Steam must be
running.

The launcher also sets the two game options the mod cannot play without, every time it starts the game: Movement Type is
forced to Keyboard (the game's own WASD walking), Evade to Cursor is forced off (otherwise Space dashes at the mouse
cursor and ignores WASD), and the keyboard-mode key map is written as the game's default one with the four walking keys
bound, which the game itself leaves empty until you press Default on its Keybinding tab. The mod's keys are built on the
game's default bindings, so do not rebind keys.

You will also need to add an exception in Defender or your antivirus of choice for the folder
`C:\Users\myusername\AppData\Local\Programs\Grimdark Controller\`. Not the executable, which is replaced by updates.  You can get
the path by right clicking the desktop icon and going to properties.  To do this for Defender, go to Windows Security ->
Virus and Threat Protection, tab over to "manage settings", hit that, then tab over to "add or remove exclusions".

## Getting Started

The control scheme is a hybrid of a standard ARPG's and a few others. The game here is that you run around and hit
stuff. Since I don't have time for a full tutorial right now I'm kind of assuming you know what Diablo is.  Grim Dawn is
like that (the game, not the accessibility features).  The mod mainly does a few things for you:

- Emulates mouse clicks on enemies, loot and so on.
- Makes the menus speak.
- Adds sonar and wall tones (not really optional).
- Telegraphs enemy attacks: as an enemy starts an attack, a short word says its shape (swing, stomp, wave, shot, ring,
  area, charge, aura, beam, frontal) from where the enemy is, so you can step out of it. The game itself has no such markers.
- Tags the level data with GPS-like information that is announced as you run around (AI-generated).

The basic flow: `.` cycles through enemies, `Alt+.` jumps to the closest one, and holding `J` attacks (`J` and `Enter`
are left clicks, `I` is a right click; in menus `Backspace` is the right-click equivalent). To explore, `V` cycles
through the exits of the current room. The game's "rooms" are more like map patches, each a few seconds to cross; a room
does not imply walls.

The mod will walk you into range of things if you are using a melee weapon, or if the thing isn't an enemy. E.g.
targeting an NPC with the targeting keys and then hitting j is usually enough to go talk to them.  Emphasis on usually:
a locked door between you and them, for example, is not something we can reliably detect.  If you want to move
specifically toward an enemy while wielding a ranged weapon, `l` is the walk-only key: it walks you there, then you die,
because standing in the enemy's face is what you asked the mod to do, so make sure to actually attack it.

`F1` opens the mod's menu anywhere; its sound glossary lists every sound the mod plays and plays each one as you arrow
over it.

Combat events are spoken by Mark and Zira. Mark is things happening to enemies; Zira is things happening to you. `T` (or
the F1 menu) opens the combat announcement settings: outgoing announcements off / brief / full, incoming announcements,
incoming hit announcements ("hit" for every attack that lands on you), and the telegraph cues with a filter for who and
which shapes speak. All of it is remembered between sessions.

You can configure sonar with `Ctrl+T` or turn it off with `\`.

The bottom row of keys (c through /) holds your information keys, which cycle through items of a given type, closest
first. Add alt to jump to closest, add shift to go backward. My personal flow is that my right hand drives these and
attacking with j and i, and my left hand drives dodging and walking with wasd and space.  You really want to read the
key list further down this file though.  Semicolon pings the thing you last targeted.  Open the map with `Ctrl+M`, then
press Enter on something, then apostrophe pings it and gives you a heading.

If you have trouble telling if a thing is north or south of you by context, `Ctrl+T` has a setting to turn on a mode
where the ping changes based on whether it's north or south that you can try. This is a bit much, so it's off by default
for now.

The general flow in other words is something like alt dot, attack with j, wait for exp announcement, alt dot to next
enemy.  It's more than that once you get going, but this is attempting to emulate the feeling of playing an ARPG so
you'll be pushing lots of buttons.  It is critical however to understand that the mod doesn't retarget when your target
disappears: when the thing you are targeting dies, or leaves your reach for more than a few seconds, the cursor stays on
the spot where it was last seen, so j, i and the number keys keep aiming there until you target something else.

The game itself assumes you already know what you are getting into and we don't yet read the tutorial announcements, so
a few things are worth knowing:

- You can have more than one character; a selector appears after you make one.
- `Ctrl+L` opens a rift. `N` to target it, `J` to interact, and you can return to town temporarily.
- Saving is automatic, but restarting the game reloads you at the last major riftgate, respawns enemies, and drops any
  rifts you opened -- so no returning to town, closing the game and coming back (if you don't have the DLC I believe
  this is always Devil's Crossing).
- To choose a class, reach level 2, press `Ctrl+N`, pick a mastery, Tab over, and select the mastery to spend a skill
  point on it. Every other build choice can be undone except this first mastery point: classes are permanent, skills are
  not. You get a second class at level 10; the class selection tab then reappears next to your first class choice.
- Levelling up is not announced because it is already obvious: the big loud clonky scare chord.
- Health and energy potions are `R` and `E`. They are not items in your inventory; they are modelled as skills every
  character has, with a cooldown, so you never run out.
- `Space` reads tooltips in menus and evades in the world. To evade:
  - Space while holding WASD dodges in that direction.
  - Space while not holding WASD dodges the way your character faces (toward the thing you are attacking, or the way you
    last walked).
  - In general you don't need evade for a long time if you play on normal.
- Lots and lots and lots of things have tooltips; get those with `Space`. To find one detail in a long tooltip, step
  through it a line at a time with `Shift+Down` and `Shift+Up`.
- You can type to search for things in menus; there's no dedicated key, just start typing.
- Many windows have more than one section so always try tab as well as arrows.
- `Ctrl+P` pauses the game (single player). While paused you can't act in the world or use menus and windows, but you
  can use the mod's keys to find out what's around you. See Pausing in the Controls section.

There are a lot of mechanics in this game, so in general Google or [the
wiki](https://grimdawn.fandom.com/wiki/Grim_Dawn) are your friends. To help you with build planning however, we have [a
table of all masteries and skills](./docs/masteries.md).  The wiki has this information but not in a very accessible
form.

Exploration requires a couple of notes.

First, the world is rotated about 50 degrees counterclockwise.  The camera defaults to an inconvenient isometric
projection where all rooms, from the perspective of a blind user, are diamonds instead of squares.  This means that NPC
dialog and guides referring to north mean northwest, northwest is west, etc.  The sighted experience is that north is
this sort of gradual zigzag between northeast and northwest, so the initial quests that say "go north" happen to sort of
actually be straight north in our orientation by coincidence, but once you get a bit further things get odd, especially
combined with only vague directions in dialog.  In practice in sighted land "go northwest" can mean quite a few
different things but should really be read "go vaguely northish and westish".

Second is that the wall tones and exit finding are not, nor will they ever be, perfect.  Sighted games have elevation
changes, irregularly shaped walls, and "wall sliding" where your character will sort of pop through gaps.  After much
pain the mod gets to like 95%, but for the last 5% you'll find oddities.  With some practice it's usually possible to
tell when it's being odd and you can probably still go that way.  A couple of notable examples of this are unfortunately
Burial Hill (the first quest you get) whose above-ground portion is an odd sort of spiral and Burrwitch Estates which
have a few rooms which aren't quite squares; they're squares where you can walk into a hallway in a non-obvious spot.
One notable possibly surprising behavior is that holding right to follow a wall to your south, for example, may move you
slightly north if the wall bends or has protrusions.  This isn't a game about super precise positioning so in general it
works out.

We are slowly adding pathfinding features. The one most relevant to exploration is that pressing `l` with an exit
targeted will move you to somewhere in the middle of the targeted room.  `l` walks to whatever you selected last, which
includes a map marker you picked, so `Ctrl+M`, pick something, then `l` walks you there if it's close enough.  If you
are stuck and don't know where to go, `p` finds the nearest area you haven't explored yet and `l` walks you to it.  The
game only knows the ground within about 200 units of you, so walking farther than that happens in steps: walk, then
press `l` again once you're closer.

## Controls

WARNING: The game's own bindings must stay at their defaults. The mod remaps them using mod-specific mechanisms.  If you
move them, bad things happen.  What bad things? It depends; don't go find out.

### Menus and windows

| Key | Description |
|---|---|
| Up / Down | Previous / next item |
| Home / End | First / last item |
| Ctrl+Up / Ctrl+Down | Previous / next group |
| Left / Right | Adjust a slider or drop-down; move along a tab row; expand or collapse a tree group |
| Tab / Shift+Tab | Next / previous panel (for example a window's tab row and its column) |
| Ctrl+Tab / Ctrl+Shift+Tab | Switch tabs from anywhere in a window. On the tab row itself, Left / Right open the tab you land on |
| Enter | Activate |
| Backspace | Secondary action: unequip an item; reclaim a skill point at a spirit guide |
| \ (backslash) | On an item in a bag, a vendor or the stash: what you have equipped in the slot it would go to -- the slot, the item and its tooltip ("nothing equipped" for an empty slot). The game's side-by-side comparison, spoken. Works on gear whose level or attribute requirements you do not meet yet (the slot is taken from the item's kind) |
| Delete | On an item in a bag: drop it on the ground around you (a whole stack drops together). Pick it back up as loot. An item that can't be dropped (soulbound or untradeable) asks whether to destroy it instead |
| Space | The game's tooltip for the item; Ctrl+Space the detailed one |
| Shift+Down / Shift+Up | The detailed tooltip one line at a time: next / previous line. The first press reads the first line; moving to another item starts over |
| Escape | Back / close |
| Letters | Type-ahead to a matching item, where the screen allows it |

The main menu has three Tab stops: the general buttons; the character list (only when there is more than one character;
Enter selects); then Start / difficulty / game mode / Delete.

### Moving and interacting

| Key | Description |
|---|---|
| W A S D | Move |
| Space | Evade (with Options -> Gameplay -> Evade To Cursor OFF: in the movement direction while moving, the way you face when standing) |
| J or Enter | Left mouse button at the reviewed thing: attack / talk / open / move, whatever a click does. Hold to hold (sustained attack). On a reviewed ground item, the game's walk-and-pick-up instead; on a reviewed door, ladder, chest, lever or shrine, the game's walk-and-use. This works even when the reviewed thing is off screen or under the HUD: the mod hands the game the target itself, and the game walks you into range for a weapon attack or casts a spell as far as its range allows, exactly as for a click on it. Blitz and Shadow Strike still need a path to the enemy, as they do for anyone. The number keys (quickbar) use the reviewed thing the same way |
| I | Right mouse button at the reviewed thing (the right-hand skill), same rules |
| Shift+J, Shift+I, Shift+number | The game's own Hold Position: attack or cast without moving. With the game's Classic Casting option on, plain J / I / number keys walk you into a spell's range and Shift casts from where you stand |
| U | Interact with the nearest usable thing within 10 units (door, chest, shrine, NPC), no aiming |
| L | Walk to whatever you selected last, finding a way round walls: the thing you last reached with the finding keys, or the map marker you picked or followed with `'`, or the unexplored area from P. On a room exit (V), walk into the middle of that room's widest part. The game only knows the ground within about 200 units of you, so a farther target may not walk until you are closer. WASD takes over again |
| G | Pick up the nearest item on the ground |
| E / R | Energy / health potion |
| F | Swap weapon set (announces "weapon set N" and the two hands) |
| Escape | Game menu |

The camera is fixed by the mod (far zoom, yaw 0); there are no camera keys. With yaw 0 the screen lines up with the
world's tile grid, so walls and corridors run straight.

The behavior of "clicking" (j, sometimes i) varies: if it's an item you try to walk to it and pick it up; if it's an
enemy you shoot at it, etc.  The game will try to move you to the destination or object when it can.  If nothing happens
you probably have to get closer, or the object is behind something.  We can't reliably distinguish the cases.

### Finding things: the review cursor

| Key | Description |
|---|---|
| . / Shift+. | Next / previous enemy, nearest first ("name level N", plus champion / hero / boss when it is one) |
| , / Shift+, | Next / previous among only the highest-rarity enemies nearby (find the boss) |
| N / Shift+N | Next / previous person or object: NPCs you can talk to; rifts, shrines, doors, levers, every dungeon entrance (a one-way exit you came in through reads "entrance, locked") |
| B / Shift+B | Next / previous bystander (NPCs without a conversation) or breakable (barrels, crates, jugs, quest destructibles -- hold J to smash) |
| M / Shift+M | Next / previous loot: items on the ground, containers |
| V / Shift+V | Next / previous exit of the current room ("blocked" if the way is shut) |
| ] / [ | Next / previous of your own pets ("Hellhound, aggressive, 2 away, 1 o'clock, 1 of 2") |
| C / Shift+C | Next / previous player character: yourself ("claude, you, 1 of 1"), and party members in multiplayer. Reviewing yourself parks the cursor on you, so a skill that drops at the cursor (Inquisitor Seal, a totem, a trap) lands at your own feet |
| Alt + . , N B M V ] C | The nearest of that group, whatever is reviewed now |
| ; | Ping the reviewed thing again: one of three sounds -- unreachable, straight line in sight, or path around -- panned, fading with distance. Also replayed automatically when the route kind changes |
| / | Inspect the target: health percent and status effects |
| \ | Sonar on / off: every nearby enemy, loot drop, breakable, devotion shrine (ruined shrines have their own sound; restored ones share the loot ping), dungeon entrance and other person or thing you can use (quest NPCs, merchants, doors, levers, riftgates, notes, graves) repeats its own ping, faster as it nears and panned to its side |
| Ctrl+M | The map: a nearest-first list of everything the game draws on it, named as the game names it -- points of interest by their own text ("Burial Hill Entrance"; a quest's marker is one of these and appears only while that quest step is active), people and merchants by name, barricades as "obstacle", the rest by the map's own words (Riftgate, Healer, Smith, Spirit Guide, hero monster, boss). The map is held at its widest zoom while the list is open, about 400 by 650 units, the same reach a sighted player gets; further away there is only the quest log's prose. Then a second Tab stop with every devotion shrine you have discovered anywhere ("desecrated shrine, Burrwitch" / "not restored, Burrwitch Village Rift, 1200 away, 3 o'clock"); Enter picks one to follow |
| ' | Follow the picked map marker: route ping plus "name, distance, bearing". After Ctrl+', puts the review cursor back on the followed thing and pings it like `;` |
| Ctrl+' | Follow the reviewed thing instead of a map marker ("following Training Dummy"). It is tracked as it moves, so in a boss fight `'` always gets you back onto the boss, wherever the review keys have been since. Picking a map marker or P replaces it |
| P | Find the nearest unexplored area you can walk to, within about 200 units, and follow it like a map marker: `'` pings it again, L walks there. Says "explored" once you have seen it, then press P for the next. "Nothing unexplored within about 200" when you have seen everything nearby (the fog left near explored ground is usually over cliffs and water); go further, or use the map. For getting unstuck when you don't know where to go next |

### Advanced targeting: moving the cursor yourself

These are **NOT** a spoken review cursor. Most players never need this. Normally the cursor sits on whatever you last
reviewed and follows it, so J, I and every cursor-aimed skill go where the review cursor is. These keys move the cursor
to a spot of your own instead: open ground for a rune, a trap, a totem or a point-aimed skill, or a little ahead of an
enemy so a mine arms before it arrives. The reviewed thing stays reviewed (`/` and `;` still answer for it); only where
a press lands changes. The next review key puts the cursor back on the thing it lands on.

| Key | Description |
|---|---|
| Shift+W A S D | Move the cursor instead of your character (the character does not move while Shift is held). The first press starts from where the cursor is now: the reviewed thing, or your own feet when nothing is reviewed. Hold to keep moving. Nothing is spoken |
| Z | Cursor mode: "cursor mode default" or "cursor mode polar", the only thing these keys say. Remembered between sessions |

In the default mode each press moves the cursor 1 unit up, down, left or right on the screen grid (the same directions
your character walks, with W toward the top of the screen). In polar mode the cursor lives on a line from your
character: W moves it 1 unit further out along that line, S 1 unit closer (never past you), and A / D swing the line 30
degrees left or right around you, keeping the distance. The cursor is always the far end of the line: S pulls it in
until it sits on you and never past you, and W from there goes back out the same way. Starting polar from your own feet
with no line yet, W goes straight up the screen.

### Information

| Key | Description |
|---|---|
| K or Ctrl+Shift+P | Where am I: position, life, region |
| H | Health and energy in full |
| X | The current room: title and description |
| Q | Objectives of the tracked quests |
| F1 | Grimdark menu (anywhere): sound glossary (every mod sound as a tree; landing on a row plays it), in the world the announcement config (T) and sonar config (Ctrl+T), and mod options: the dev server on/off (off by default; Enter flips it now and remembers it) and check for updates on/off (on by default; from the next start) |
| T | Combat announcement settings. First Tab stop: outgoing announcements off / brief (just "hit", "crit", "miss", "blocked") / full (the numbers), incoming announcements (your health, effects on you) on/off, incoming hit announcements ("hit" for every attack that lands on you) on/off, and telegraph cues with four states: off, your target (only the enemy you are reviewing or fighting), highest tier (only the strongest kind of enemy nearby, so a pack's boss speaks and its adds do not), all. Enter cycles, Left/Right step. Second Tab stop: one on/off row per cue shape (swing, stomp, wave, shot, ring, area, charge, aura, beam, frontal). Third Tab stop, targeting: prefer enemies in line of sight (on by default) -- the enemy review keys list enemies you can see first, but one you cannot see only counts as 10 units farther away, so a close enemy round a corner still comes first. Space on that row explains it. Escape closes; everything is saved between sessions |
| Ctrl+T | Sonar config. First Tab stop: one on/off row per positioned cue -- wall tones, harmful ground (all three of its sounds), and the sonar's enemy, loot, entrance, breakable, shrine and interactable pings, and the ticks of enemy projectiles in flight (Enter flips); then two pitch options, with Space explaining each: raise enemy pitch in range (on by default: the enemy ping rises one step within 20 units and another within melee reach, about 3), and the north / south echo on route pings (off by default: ; and ' play their sound a second time just after, higher for a target to the north, lower for one to the south, no echo when it is nearly level). Second Tab stop: seven volumes in percent -- wall tones, harmful ground, enemy pings, other pings, the enemy voice (Mark), your voice (Zira) and projectile ticks (Left/Right by 5; Enter steps up and wraps); every step is 3 decibels, so the steps sound even, and plays that channel's sound at the new level so you set it by ear. Escape closes; everything is saved between sessions. The bare backslash still switches the whole sonar off and on |

Spoken automatically, by position: damage numbers, misses, dodges and blocks from where they happen; your health at
every 10 % step; debuffs put on you; kills and experience; place changes ("Devil's Crossing, the prison, cell block
corridor" -- the first part is the game's own area name, exactly what the minimap shows a sighted player there). The
game's banners (level up, quest updated) and its "skill not ready" style popups are read once each.

### Skills and the quickbar

| Key | Description |
|---|---|
| 1..9, 0 | Quickbar slots |
| Y | Switch quickbar (announces "quickbar N") |
| Ctrl+1..0 | Read quickbar slot 1..10 of the displayed bar: the skill and how it aims ("Cadence, at a target", "War Cry, around you", "Overguard, self") |
| Ctrl+- / Ctrl+= | Read the left / right mouse skill |
| Ctrl+` | Hotbar manager: both bars and the mouse buttons of the current weapon set; activate a slot to pick a learned skill, or clear / default |
| Alt (held) | Show item labels |
| Alt+Enter | Blocked ("Fullscreen must be toggled through the options menu to avoid a vanilla crash"): the game's own toggle crashes it in the world; use Options, Video, Window Mode |
| O | Show all items on / off ("showing all items" / "loot filter on"): while on, the loot review group (M), the loot sonar and the game's own labels ignore your loot filter -- the same as holding Alt, latched |
| F2..F6 / F7 | Select pet 1..5 (toggle) / select all pets, announced; the selection applies to the next pet command only |
| Shift+Backspace | The selected pets (all, if none are selected) attack the locked target |

### Pets

Pets are announced as they come and go ("Hellhound summoned", "Hellhound down") and never count as enemies. The game's
own "Pet Attack" skill (all pets attack the cursor's target, or move to a point) can be put on a quickbar slot from the
hotbar manager and works against the locked target like any aimed skill.

| Key | Description |
|---|---|
| Backspace | The pet overlay: one row per pet, "name, stance, selected". Left / Right change the stance (normal, aggressive, defensive -- shared by every pet of that summoning skill, remembered across resummons), Enter toggles selected, Backspace disbands, Space says where it is. Below the pets: "attack locked target" and "recall", for the selected pets or all; a command closes the overlay |

### Windows

| Key | Description |
|---|---|
| Ctrl+C or Ctrl+I | Inventory, equipment, and stats |
| Ctrl+N | Skills: one tab per mastery, then constellations |
| Ctrl+Q | Codex |
| Ctrl+J | Factions |
| Ctrl+L | Personal riftgate |
| Ctrl+1..0, Ctrl+J, Ctrl+I | Inside inventory / skills: put the focused skill (or, on a weapon slot, the weapon's basic attack) on quickbar slot 1..10 / the left mouse / the right mouse |
| Ctrl+O | Loot filter config |
| Ctrl+P | Pause / unpause (single player only), see Pausing below |

### Pausing

`Ctrl+P` is the game's own pause, single player only; press it again to unpause. It has always been there, but it was
easy to miss because plain `P` does nothing with the mod running. While paused:

- Everything in the world freezes: monsters, projectiles, cooldowns, buffs.
- You cannot act in the world. Moving, attacking and using skills are all refused, including skills you place at the
  cursor (runes, totems, traps).
- Menus and windows do not work while paused: inventory, character sheet, skills and devotion, the map. Unpause to sort
  gear or spend points.
- The mod's own keys keep working, since they don't need the game clock: the review keys (`.` `,` N B M V and the
  rest), `;` and `/`, `X` for the room, and reading things. So you can pause in a fight, find out what is around you and
  where, and then unpause.

### Controller

An Xbox controller (or anything Windows sees as one) and the PlayStation DualShock 4 and DualSense, over USB or
Bluetooth, work alongside the keyboard. The mod reads the controller itself and the game never sees it, so the game stays
in its keyboard mode. "Controller connected, Xbox" or "... PlayStation" is said when one appears. The controller only
works while the game window has the focus.

For a PlayStation controller, turn Steam Input off for Grim Dawn (in Steam: Grim Dawn, Properties, Controller, "Disable
Steam Input"). With it on, Steam hides the controller from the game, and the mod lives inside the game, so it never hears
the controller connect.

Buttons are written Xbox / PlayStation. Every controller command is one of the keyboard commands above; the key is
given in brackets.

In the world:

| Button | Description |
|---|---|
| Left stick | Move (W A S D, eight directions) |
| B / Circle | Evade (Space). B evades in every layer below too |
| A / Cross | Left mouse button at the reviewed thing; hold to keep attacking (J) |
| X / Square | Right mouse button at the reviewed thing (I) |
| Y / Triangle | Jump to the nearest enemy (Alt+.) |
| LB / L1, RB / R1 | Health potion (R), energy potion (E) |
| LT / L2 (hold) | Hold Position: attack or cast without moving (Shift) |
| RT / R2 (hold) | Quickbar layer, below |
| Right stick flick | The scanner, below |
| R3 | Ping the reviewed thing (;) |
| L3 tap | Where am I (K) |
| L3 hold | Mod layer, below |
| D-pad up / down / left / right | Health and energy (H) / use the nearest usable thing (U) / pick up the nearest item (G) / walk to the last selection (L) |
| Start / Options | Tap: game menu (Escape). Hold half a second: pause or unpause (Ctrl+P) |
| View / Create, or the touchpad click | Quick menu: a list of the windows and settings (inventory, skills, map, codex, factions, riftgate, loot filter, pets, hotbar manager, combat announcements, sonar settings, cursor mode, pause, controller controls (this list of every button), the Grimdark menu); Enter (A) opens one |

The scanner: flick the right stick up or down to change category, which says the category and lands on its nearest
entry ("loot, Rare Belt, 6 away, 2 o'clock"); flick left or right for the previous or next entry, nearest first. The
categories, in order: enemies, strongest enemies, people and objects, loot, exits, bystanders and breakables, pets,
characters. They are the review groups of `.` `,` N M V B ] C, so a landing moves the review cursor exactly as those keys
do.

Quickbar layer, hold RT / R2: A, X, Y = slots 1, 2, 3; LB, RB = slots 4, 5; d-pad up, right, down, left = slots 6, 7, 8, 9;
R3 = slot 10. Hold LT as well to cast without moving.

Mod layer, hold L3 (the d-pad is under the same thumb, so it has no L3 commands):

| L3 + | Description |
|---|---|
| A / Cross | Follow the picked map marker (') |
| X / Square | Follow the reviewed thing (Ctrl+') |
| Y / Triangle | Inspect the target (/) |
| LB / RB | Swap weapon set (F) / switch quickbar (Y) |
| LT / RT | Sonar on or off (\) / show all items on or off (O) |
| Right stick flick up / down / left | Objectives (Q) / describe the room (X) / nearest unexplored area (P) |
| R3 | The right stick moves the cursor instead of scanning ("cursor"); again to go back ("scanner"). Grid or polar is the quick menu's cursor mode (Z) |
| Start / Options | Grimdark menu (F1) |

In menus and windows:

| Button | Description |
|---|---|
| D-pad or left stick | Up / Down / Left / Right (held repeats) |
| A / Cross | Activate (Enter) |
| B / Circle | Back (Escape); Start / Options too |
| X / Square | Secondary action (Backspace) |
| Y / Triangle | Tooltip (Space); L3 + Y the detailed one (Ctrl+Space) |
| Right stick up / down | The detailed tooltip a line at a time (Shift+Up / Shift+Down) |
| R3 | Compare with what you have equipped (\) |
| LB / RB | Previous / next panel (Shift+Tab / Tab) |
| LT / RT | Previous / next tab (Ctrl+Shift+Tab / Ctrl+Tab) |
| L3 + LB / RB | Previous / next group (Ctrl+Up / Ctrl+Down) |
| L3 + LT / RT | First / last item (Home / End) |
| L3 + A | Actions for the focused thing: assign to the left or right mouse or a quickbar slot, sell part of a stack or set the receiving bag, drop |

Typing (a character's name, type-to-search in menus) still needs the keyboard.

# Development

For development the game runs **visible but never focused, with game audio and speech muted**, and it is driven over a
local HTTP dev server inside the DLL (port 8791, `GRIMDARK_PORT` to change) -- so iterating on the mod never fights the
developer's screen reader. The server is off by default: F1 -> mod options turns it on for a player who wants to debug
(it listens on localhost only, but its routes can teleport, cheat and run Lua, so nothing starts it unasked); the dev
launcher sets `GRIMDARK_PORT`, which turns it on for that game process. Requires the game NOT to be running already, and
[uv](https://docs.astral.sh/uv/) for the Python tooling (`uv run` installs Python 3.12+ and the dependencies from
`pyproject.toml` on first use):

```
uv run tools/gd.py launch            # build, launch unfocused + muted with the DLL injected before init, wait for /health
uv run tools/gd.py launch --speak    # same, but audible
uv run tools/gd.py status            # running / CRASHED / hung
uv run tools/gd.py speech --since 0  # what the mod has spoken
uv run tools/gd.py key enter         # synthetic key events (also: keys "text", click X Y, cursor X Y)
uv run tools/gd.py log --since 0
uv run tools/gd.py kill
```

Do not restore or click the game window during a dev session: it activates itself and takes the keyboard. `gd.py`
without arguments lists every command; the dev routes and the hot-reload loop are in `CLAUDE.md`, the dated
implementation notes in `docs/devlog.md`, the mechanisms in `docs/*.md`.

Environment variables read by the DLL: `GRIMDARK_ANY_VERSION=1` (skip the version gate on an unknown game build -- for
measuring a patch, expect crashes), `GRIMDARK_PORT` (dev server port; set = the server starts), `GRIMDARK_MUTE=1` (mute
game audio and speech), `GRIMDARK_NOFOCUS=1` (block the game's own focus grabs, dev only), `GRIMDARK_UPDATE_HOST` (replaces
api.github.com for the update check, dev only: `nonexistent.invalid` tests the offline path), `GRIMDARK_HOOK_WIDGETS=1`
(experimental, crashes the game -- leave unset).

## Building

- Visual Studio 2022 Community with the "Desktop development with C++" workload (MSVC 14.44 is what the author uses; the
  game's ABI is MSVC, so no other compiler will do). CMake and Ninja are installed by that workload; `tools/vsdev.cmd`
  finds any VS 2022 edition (Community, Professional, Enterprise, Build Tools) through vswhere. A bare Build Tools
  install ships no Ninja of its own, so put one on PATH in that case.
- With a Windows SDK older than 10.0.22000 (Build Tools' default is 10.0.20348) the SDK's C++/WinRT headers predate
  C++20 coroutines and do not compile; the first configure then downloads the pinned C++/WinRT NuGet package (hash
  checked) and generates newer headers into `build\ninja\cppwinrt\` (~55 MB). A newer SDK needs nothing extra.
- The build links the C runtime statically, so a player needs no Visual C++ redistributable; only `prism.dll` has to sit
  next to `grimdark.dll`.
- No other downloads: the prism speech SDK (the x64 headers, import library and `prism.dll` of release v0.18.1),
  Detours, miniaudio, SQLite, doctest, the room database and the audio assets are all in the repo.

From the repo root, in any shell:

```
tools\build.cmd
```

The first run configures a Ninja RelWithDebInfo build in `build\ninja\`; later runs just build. Output:

- `build\ninja\grimdark.dll` -- the mod
- `build\ninja\gdlaunch.exe` -- the player's launcher
- `build\ninja\gdinject.exe` -- the injector (dev: inject into a running game)
- `build\ninja\prism.dll` (the screen-reader speech library) and `build\ninja\assets\` -- copied next to the DLL at
  build time; the DLL loads them from its own directory, so keep the folder together.
- `build\ninja\gdcore_tests.exe` -- unit tests for the engine-free core; run with `cmake --build build/ninja --target
  check` (inside `tools\vsdev.cmd`, or any VS developer prompt).

On success MSVC and Ninja print very little; check the exit code.

CI (`.github/workflows/build.yml`) does the same on a clean Windows runner for every push and packages the player zip
with `tools/package.py` (artifact `grimdark`: `gdlaunch.exe`, the DLL, prism, the injector, `assets/`, this README and
the licenses; the PDB is the `grimdark-pdb` artifact). A second job builds `GrimdarkInstaller.exe` from `installer/`. A
`v*` tag publishes the zip, the installer and the PDB as a release; every push to `main` refreshes the `ci-latest`
pre-release.

## License

The mod's own code is by Austin Hicks, under the zlib license (`LICENSE`). Third-party components and their licenses are
listed in `third_party/README.md`. `tools/exports/` holds the game's DLL export tables (symbol names only), generated
from the installed game.
