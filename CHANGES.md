# 0.6.0-pad.1 (2026-11-01)

- New: controller support. An Xbox controller, a DualShock 4 or a DualSense, wired or Bluetooth, works alongside the keyboard; the game itself never sees the controller, so nothing changes in how it plays. The left stick walks, B or Circle evades, A and X attack and cast at what you are reviewing, Y jumps to the nearest enemy, flicking the right stick runs a scanner through enemies, loot, exits, people and more, and View or Create opens a quick menu of the windows. The README's Controller section has every button.
  - The controller only works while the game window has the focus.
  - For a PlayStation controller, turn Steam Input off for Grim Dawn (Steam, Grim Dawn, Properties, Controller); Steam otherwise hides the controller from the game and the mod with it.
  - Typing a character's name still needs the keyboard.
- This build is the Grimdark Controller fork: it has its own installer, `GrimdarkControllerInstaller.exe`, installs as "Grimdark Controller" with its own desktop icon, and gets its updates from the fork's releases. It can sit next to the original Grimdark, and both share the same settings.

# 0.6.0 (2026-10-26)

- Telegraph cues are more accurate:
  - New cue "aura": an enemy is wrapping itself in something harmful that moves with it for a few seconds, such as Kilrian's ring of fire. Keep away from the enemy. It has its own on/off row in `t`.
  - Fire pools, sigils and lightning strikes now say "area" instead of "shot": they land on the spot you were standing on when the enemy started casting, so move off that spot.
  - Lobbed attacks now say "area" too: things thrown in an arc onto the spot you were standing on, such as the Grand Priest's fireballs, ice bombs, molotovs and acid spit.
  - Fans of projectiles fired in front of the enemy, such as Condemn, now say "wave" instead of "shot": step sideways out of them.
  - New cue "frontal": a wide spread of projectiles covering a quarter circle to a half circle in front of the enemy, such as ground spines, chaos barrages and the half-circle novas. Get behind it or well to the side. Anything wider says "ring". It has its own on/off row in `t`.
  - New cue "beam": a zap that locks on to you, such as an Aether Crystal's. Moving won't dodge it once it starts; getting out of range or out of sight before it does will. It has its own on/off row in `t`.
  - The sound glossary in the F1 menu now has the area, charge and aura cues.
- New: enemy projectiles make a sound. Slow ones you can step around, such as fireballs, orbs and deathbolts, tick from where they are while they fly, so you can hear them travel, including the ones that will miss you; one that is going to hit you ticks higher. Something lobbed in an arc, such as a grenade, plays its own landing sound from the spot where it is going to land instead of ticking from the air, and ticks higher when you are inside its blast, so you know which way to step out of it. Fast ones such as crossbow bolts, which arrive too quickly to dodge, tick once, more quietly, from where they were fired, so you know where the shooter is. Your own projectiles and your pets' stay silent.
  - `ctrl t` has an on/off row and a volume for projectile ticks, and the F1 sound glossary has the tick, the landing sound and their higher versions.
  - Ground waves tick the same way: a crack, fire wave or shockwave an enemy sends along the ground, such as Voldrak's Groundbreaker, ticks from where it is as it travels, and higher when it is heading at you.
  - Projectiles fired all at once, such as a ring of ice spikes, tick a moment apart so you can hear how many there are, instead of sounding like one.
  - This is a first version: in a big fight it can get busy, and the sound and levels will change.
- New: harmful ground that enemies create, such as a Skeletal Warlock's sigil or a pool of fire left by a fireball, now sounds like the other harmful ground: you hear which way it is, and the warning sound while you stand in it. Before, only harmful ground that is part of the map made a sound.
- New keys `shift down` and `shift up`: in menus and windows, read the detailed tooltip one line at a time, so you can find one detail without listening to the whole thing. The first press reads the first line; moving to another item starts over from the top.
- Fix: the README listed `shift up` and `shift down` for jumping between groups in menus; the keys are `ctrl up` and `ctrl down`.
- New key `delete`: on an item in your bags, drops it on the ground around you, where you can pick it up again like any loot. A whole stack drops together. Items that can't be dropped (soulbound or untradeable) ask whether to destroy them instead, the same as in the game.
- Fix: capital W, A, S and D can be typed in text boxes, such as the character name. Before, `shift w`, `shift a`, `shift s` and `shift d` typed nothing there.

# 0.5.4 (2026-10-09)

- Works with Grim Dawn 1.3.1.1 (the October 8 hotfix). The previous game version is no longer supported.

- Fix: `l` walks to places that sit just off the walkable ground, such as a mine or cave entrance picked from the map. Before, the game refused the walk and nothing happened.
- Fix: skill tooltips at a spirit guide (reclaiming skill points) are the full ones again, including what the next level gives, followed by the reclaim cost.

# 0.5.3 (2026-10-05)

- Fix: equipping a two-handed weapon while holding a weapon and a shield no longer loses the two-handed weapon. Before, it disappeared, and the old weapon and shield showed up in your bag while still being listed in your hands. Now both go to your bag and the two-handed weapon goes in your hands, the same as in the game. This works both with `enter` in the bag and from the Right Hand slot.
- Fix: equipping a two-handed weapon from the Right Hand slot no longer leaves your shield in the other hand.
- If your bag can't fit what you take off, equipping now says "no room in your bags" and nothing moves. Before, the item that didn't fit could be lost.

# 0.5.2 (2026-10-02)

- Fix: the update announcement now waits until the main menu has been up for a few seconds, so the game's own startup speech no longer talks over it.

# 0.5.1 (2026-10-02)

- Nothing new for players: this release exists so the update check added in 0.5.0 has something newer to find.

# 0.5.0 (2026-10-02)

- Grimdark now checks for updates when it starts and tells you if a newer version is out, for example "Grimdark update available v0.5.0, you have v0.4.0, run the Grimdark installer to update". If you installed a CI build, it tells you about newer CI builds instead. It stays quiet if you are offline. Turn it off in `f1`, mod options, check for updates.
- At a spirit guide, the constellations tab has a new "clear all constellations" row that says what it costs. Clicking it asks yes or no, then gives back every devotion point at once for the same price as reclaiming them one by one. This gets you out of constellations that hold themselves up, which the game otherwise only lets you undo with a Tonic of Clarity. Celestial powers need binding again afterwards.
- New key `ctrl apostrophe`: follow whatever you are reviewing instead of a map marker. After that, `apostrophe` puts your review cursor back on it and pings it like `semicolon`, even if it moved or you have reviewed other things since, so you can keep targeting one enemy in a boss fight. It says "gone" once the thing no longer exists. Picking a map marker or pressing `p` replaces it.
- The stats tab now tells you how to spend attribute points. The attribute points line always says to click Physique, Cunning or Spirit, and while you have points left each of those says "click to assign an attribute point".
- Fix: Vire's Might, Nullification and the medal runes that charge, leap or teleport to a spot now show up in the hotbar manager. They fire at your target, or at the spot you have selected.
- Fix: attaching a component or augment only offers items in your bags or equipped, no longer items in your personal and shared stashes.
- Items now say "with component", "with augment" or "with component augment" in every list, not just the inventory: the vendor's buy and sell tabs, both stash lists, the equip and attach pickers, and the inventor.
- `ctrl p` pauses the game in single player. It always worked, but it wasn't written down anywhere. The README has a new Pausing section on what you can and can't do while paused (short version: no acting in the world and no menus or windows, but the mod's own keys work).
- `l` now walks to whatever you selected last: the thing you found with the finding keys, a map marker you picked or followed with `apostrophe`, or the unexplored area from `p`. Targets beyond about 200 units may not walk until you get closer.
- New key `p`: find the nearest unexplored area you can walk to, within about 200 units. `apostrophe` pings it and `l` walks there; it says "explored" once you've seen it. Meant for when you're stuck and don't know where to go.
- Fix: the hotbar manager's skill list now uses the game's own rules, so it no longer offers skills the game won't put on a bar, such as auto-toggled skills or skills from the weapon set you aren't holding.
- Fix: skill tooltips in the skills screen no longer claim you can add points when you can't. They now say why, just like the game: no points, mastery too low, skill maxed, needs another skill first, and so on. Tooltips in the hotbar and celestial power pickers no longer talk about skill points at all.

# 0.4.0 (2026-09-27)

- Wall tones are now smoother.  When hugging a wall the wall will seem flatter/straighter.
- A number of critical but rare incorrect wall tone cases have been fixed.
- With `alt dot` and the other enemy targeting keys, the mod now prefers to target the nearest enemy you can fire at when enemies are bunched together, rather than grabbing one behind a wall.  Turn this off in `t`'s new targeting section.
- We now explicitly support classic casting.  This mode, enabled in the game's options menu, walks you into range of enemies before using ranged abilities. Note that the options menu requires tabbing to Apply and pressing it; note also that this mode does not give you control over how you'll move.
  - Hold shift in classic casting if you want to avoid moving (vanilla behavior, not something we added).
- `l` is the "move to" key. Anything you can target with the review cursor that is reachable can be moved to.  This explicitly excludes map icons.  Notably it includes adjacent room entrances, in which case it'll move you to the approximate center of the target room.
  - IMPORTANT: sometimes, rarely, a room is divided by a wall and the path to it thus goes through other rooms, possibly lots of them. You'll walk it, but maybe into a crowd of enemies.
- `j`, `i`, and the other skill use/clicking keys no longer care about the game's HUD or camera.
- When enemies are in range of ranged abilities, their sonar ping goes up in pitch. When they are in melee range, it goes up in pitch again. Turn this off in `ctrl t` if you don't want it.
- If you have trouble telling if enemies and other objects are north or south of you, a new option in `ctrl t`, "north south echo on route pings", may help by changing the ping you hear when using `semicolon`, `apostrophe`, and the other targeting keys.
- Fix: automatic loot pickups are now announced through Zira.
- Untested fix: Unicode in the map list may now render properly, and Unicode when using type-ahead may now work.
- Fix: do not play mod-provided sounds for shrines that are not enabled on your difficulty.
- Fix/improvement: rework how the 3 pings you get when cycling through entities work out reachability, to better match both how you use them and what's really going on.
- Fix/improvement: the stats screen now shows armor breakdown
- Fix: `alt comma` works again, jumping to the nearest of the highest-rarity enemies like `alt` with the other review keys.
