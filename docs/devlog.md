# Grimdark devlog

The dated status notes that lived in CLAUDE.md until 2026-09-20, moved here verbatim so the instructions file stays
short. Roughly chronological, one bullet per piece of work; each says what was built, what was verified live and
what was left open at the time. Newer work goes at the END of this file. The durable lessons are distilled in
CLAUDE.md "Traps and lessons"; the mechanism docs are `docs/*.md`.

## 2026-08-21 onward

- Working: dev loop (launch unfocused/muted, hot reload, /text /speech /gui /action /click /key /gamekeys),
  engine-free core (`src/core`: MessageBuilder, strings, input manager, screen framework, ported nav graph +
  announcer + typeahead + navigator; 76 doctest cases), app layer wiring, `unsupported` fallback, and the
  screens `main_menu`, `create_character` (Name edit mode: Enter edits, letters go to the game's field with
  per-character echo, Enter/Escape/Tab/arrows leave; Next disabled until a name exists; state survives the
  Difficulty dialog's Back, resets after Cancel -- both measured), `difficulty_select` (two variants: Create/Back
  after Create Character, Accept/Cancel from the main menu's difficulty button; locked tiles read from the
  greyed label colour; Space reads the description block). Whole flow verified through the loop: main menu
  -> Create -> name -> Next -> Normal -> Create lands back on the main menu with the character selected and
  "Start" enabled (the game does NOT auto-start).
- Modifier state comes from the game's per-event flags (ButtonEvent +17/+18/+19 = shift/alt/ctrl), never from
  our own down-tracking: alt-tabbing into the game delivers an Alt key-down whose release goes to the task
  switcher, which left Alt "held" and silently killed every plain binding (fixed 2026-08-21; tracked keys are
  also reset on any foreground change). `/keydown?name=alt` then `/key?name=down` is the regression test.
- Keyboard ownership: `Screen::owns_keyboard()` / `passes_key()` decide per frame who sees key events; the
  key poll hook filters per event (`set_game_key_filter`). Foreground faking is caller-filtered
  (`caller_is_game`): game modules see the window as active, dinput8.dll sees the truth (verified by the user:
  background keys no longer reach the game; Ctrl+letter chords arrive with ctrl/shift flags set).
- Known: the name field's text is NOT captured by the RenderText2d hooks (the edit widget draws another way),
  so the name is our state only. `description()` in difficulty_select.cpp and the name-field click offset
  are measured at 1600x900. The main menu's two icon buttons bottom-left (exit at 48,871, options at 95,871)
  are not modelled yet (no text label).
- In the world (verified 2026-08-21, character "Bob", Normal): the tick runs (`Engine::Update`), the
  `unsupported` fallback speaks and passes the keyboard to the game, HUD text is captured. The game's
  "Welcome to Grim Dawn v1.3" message box (title lines + "Okay") is a good first generic in-game screen.
- In-game layer (2026-08-21, all verified through the loop): WASD is the game's own `movementType = 1`
  (`HandleActionFromJoystick` from exe+0x2c708 while moving); `src/world.cpp` reaches the player, region,
  life, camera yaw, navmesh probes and nearby entities with class names; `src/screens/in_game.cpp` runs four
  ear-fixed wall tones (miniaudio) and Ctrl+Shift+P "where am I"; `world::lock_target(id)` parks the virtual
  cursor on an entity and the game hovers/targets/attacks it natively (a click at the projection produced
  `HandleActionFromMouse(false, true, ..., id 15984)` on the training dummy). `screens/message_box.cpp` models
  the game's Okay / Yes-No boxes (untested: the welcome box was dismissed for good).
- Screens added 2026-08-21 evening, all verified through the loop: `pause_menu` (Escape in the world: Return to
  Game / Options Menu / Exit to Main Menu / Quit to Desktop; Escape = Return), `loading` (the tick DOES run
  through a load; speaks "loading" + the tip line), `message_box` (first live use: the exit confirm), `tip`
  (tutorial tips as a mod-owned overlay: the `LocalizationManager::GetText` hook records `tagQuickTip*`
  fetches, the screen recognises the popup's title line top-left, reads the lines as items, Close/Escape
  right-clicks the popup -- UNTESTED live, no tip has fired since). The `unsupported` fallback now speaks
  only after 30 frames current (transition frames were noise). Round trip world -> pause -> Exit -> Yes ->
  main menu -> Start -> loading -> world works entirely through our screens.
- Labels: `world::label_of(id)` / `/entities` give the game's own names ("Hangman Jarvis", "Chester",
  "Training Dummy", the player's name) via `Monster::GetGameDescription(false,false)`,
  `Npc::GetRolloverDescription`, `Player::GetRolloverDescription`, `Item::GetGameDescription` (u16 by value,
  hidden pointer, SEH-guarded). **Wall-tone probe = the navmesh RAYCAST** (2026-09-03, `world::free_distance_ray` =
  `NavManager::FindStraightMovePoint`, one per lane, side lanes gated by `FindClosestPointOnPathMesh`; verified live at
  Burrwitch Outskirts (-459.5,-951.5): north read 15 by the old probe and 0.9 by the raycast, WASD north did nothing).
  **TRAP: `NavManager::IsPointOnPathMesh` is a bounding-box test, not containment** -- findNearestPoly + "a polygon was
  found", and Detour's findNearestPoly collects candidates by BV-box overlap without bounding the distance, so any hole
  narrower than the polygons around it (rocks, pillars, trees; the checkerboard caps polygons at 8 u) reads walkable.
  Never use it for a player-facing decision. Still on it: `world::route_kind_to` (the review cursor's straight / path /
  unreachable word -- can say "straight" through a rock; not yet switched), rooms.cpp's two permissive pre-filters ahead
  of `find_path`, and dev routes (/navprobe, /teleport, /blocks). docs/re_wall_sliding.md section 7 has the measurements.
  Wall tones (reworked 2026-09-01, `docs/re_wall_sliding.md`): ONE bank (wotr's set 2, assets/audio/walltones/2;
  set 1 vendored, unused), world-fixed compass directions (the camera is pinned to yaw 0 = grid-up, which the tones call north; no yaw), per-frame
  RECTANGLE probes -- 7 parallel lanes 0.5 u apart per direction, free distance = the FARTHEST lane, so a wall is a
  wall only when the whole 1.5 u half-width hits it and silence means "you can go this way, mostly straight" (the
  WASD command walks to a point 1.25 u ahead and the navmesh snap pulls the character into any opening laterally
  closer than that, and round a corner the diagonal: a 1.0 u half-width missed a corner hop live, 1.5 caught it) --
  range 10, loop gain 1.0, 250 ms watchdog. Trims: flat except south +3 dB, BY EAR -- the 2026-08-22 dB(A) match
  made set 2 too quiet for the user (meters undercount its brighter timbre). `/walltones?range=&vol=&lanes=&
  trim=off|default|<n|e|s|w>,<dB>&time=N`. The old wall/obstacle classifier (`world::classify_block`, mesh 2-4 u
  beyond the stop) measured thickness, not the blocker, and is dev-only (`/blocks`).
- Controls (2026-08-21, docs/controls.md has the full default map, read from screenshots of Options ->
  Controls): the game binds single buttons only, so "lifting to chords" is done on our side -- in the world
  the `in_game` screen owns the keyboard, passes the frequent keys straight through (WASD, 1-0, Space, E, R, U,
  Escape, Alt, F2-F7) and app.cpp's `game.*` actions map Ctrl+<default key> to an injected plain key
  (verified: plain C swallowed, Ctrl+C opens the character window). The character window's text is NOT
  captured by the text hooks (another widget path, like the bindings list and the name field).
- Tip overlay verified live (the "Character Window" tip): reads, lists lines, right-click closes -- but a
  modal game menu on top eats the click; lines are drawn twice (deduped in tip.cpp).
- Review cursor (2026-08-21, wotr's scanner keys, verified through the loop): `.`/`n`/`b`/`m` (+Shift back)
  cycle enemies / people / bystanders / objects nearest-first, speak "label, N away, H o'clock, i of n", and
  lock the virtual cursor on the landing; `i`/Enter click it; `k` = where am I. Identity = object id,
  re-found each step (wotr's continue-from rule). Enemies = Monsters that `FactionManager::IsFoe(playerId,
  id, false)` (via `GameEngine::GetFactionManager`) calls foes -- guards are Monsters too. Objects = labelled
  non-character entities only (engine helpers ScriptEntity/PatrolPoint have no label); nothing qualifies at
  the spawn yet -- chests/doors/items need a class survey. docs/controls.md has the player-facing table.
- Structured menus (2026-08-22, verified through the loop): `src/exe_ui.cpp` reads the exe's widget objects
  (RVAs + offsets from static RE with `tools/exe_dis.py`, which now resolves imports/xrefs/strings offline;
  `docs/exe-ui-layout.md` is the reference). `main_menu` (12 widget-backed items incl. the unlabeled
  Options/Exit icons), `create_character` (name read from the edit box, Male/Female/Hardcore from the toggles'
  pressed byte, Next's enabled byte), `difficulty_select` (tiles pressed/enabled, description from its text
  widgets), `message_box` (DialogManager in the world, the menu popup window at the main menu) and
  `pause_menu` (the exit window's four TextButtons pressed through its registry) no longer touch screen
  space. Round trip main menu -> Create -> Difficulty -> Start -> world -> pause -> Exit -> Yes -> main menu
  runs on those paths. `app.cpp` skips key dispatch on the frame a screen becomes current (structured
  detection is immediate; the Escape that opens the pause menu would otherwise close it). Dev: `/ui`,
  `/ui/activate?ptr=`, `/ingame`, `/dialog[?answer=]`. Synthetic `/key` events are served to the game AND
  the mod, so dev-driven Escape closes a menu dialog natively before our Back runs (not a bug for real keys).
  The exe layout check runs on first use (injection happens before SteamStub unpacks the code).
- UI pass completed 2026-08-22 (all verified through the loop): `delete_character` (prompt, the DELETE edit box,
  Accept/Cancel), `options` (7 tabs named from their rollover tags via `hooks::localize`; pages of check boxes
  with tooltip descriptions, sliders (Left/Right 5 %, large 20 %), drop-downs (Left/Right), the key-binding
  table read-only; Apply/Default/Close), `tip` (read from the exe's tip manager, Close = the right-click's
  state write), `conversation` (the conversation window: speaker, speech, rows; choosing = a click at the
  row's own rect so the step's quest actions run), `loading` (app state 10), `in_game` (InGameUI present).
  `screens/edit_field.h` (EditSession) is the shared typing-into-a-game-edit-box behaviour. `textcap` is now
  dev-only (`/text`) except the loading tip line. The navigator rerenders before rebaselining its live watch
  after activate/adjust (values were being spoken twice). Key-binding REBINDING, the Options discard prompt on
  Close (not seen yet), the Multiplayer/Network screens and the in-world windows (character, inventory,
  skills, quests, journal -- framework B, offsets in docs/exe-ui-layout.md's InGameUI map) are not modelled.
- In-world input, decided 2026-08-22 (explicitly deviating from wotr: the player is always embodied, perception
  and interaction are what the camera shows, the camera is the player's): **J = left mouse button, I = right,
  Enter = left, U = the game's own Interact (nearest usable thing within 10 units, no aiming); hold = hold.**
  The button goes down at the virtual cursor (the locked review target while the camera shows it, else the real
  cursor) and the game decides (attack / talk / open / pick up / skill; click-to-move is off with
  `movementType = 1`). Hold semantics (static RE, `re_mouse_hold.md`): the exe re-issues the command every
  10-50 ms while a button is held and tears it down on the first event with both buttons up, so
  `hooks::set_mouse_hold` injects the transition once and then patches every REAL polled event to report the
  button held (+0x10/+0x11), active (+0x18) and at the override position. Verified: holding J on the locked
  dummy walked the character 22 units to it and attacked continuously (100 `HandleActionFromMouse` calls in 3 s).
  NPC talk / pickup / use are one-shot in the game (they clear the command themselves). A locked thing the
  camera does not show: "too far away" on the key, nothing sent; the review readout says "distant" for it and
  the cursor override is only parked while it is on screen. **Camera locked** (`world::pin_camera`, per frame
  from the in-game screen; the user found zoom does not change what they hear): `GameCamera::SetZoom` at the
  far end of its range (+0x590..+0x594, current fraction +0x584) and `SetCameraYaw(0)` = grid up (NOT the game's north, see the 2026-09-11 compass note), re-applied
  when the game drifts them; the zoom preset keys were removed. Far zoom brought the 25-unit-away dummy on screen. The Interact key (`tagUse`, action 0x36) and Pickup
  (0x37) are proximity searches, not cursor actions (`re_interact_key.md`); `tools/arc_unpack.py` reads the
  game's localized strings offline. Review groups now use the game's own predicates (2026-08-22): people =
  `Npc::HasConversation()`, bystanders = Npcs without one, objects = `FixedActor`/`Item` whose virtual
  `IsOfInterest()` is true (the Interact key's filter; slot found in each class's exported vftable, is-a via
  `RTTI_ClassInfo+0x10` parent walk) -- Hangman Jarvis is a person again; near the spawn only the riftgate
  (47 units) and a lore note qualify as objects, the locked gate and the checkpoint do not. Dev:
  `/scan?group=0..3&max=`, `/classinfo`, `/entities` marks `[FixedActor|Item interest|no-interest]`, `/key`
  knows period/comma/semicolon/... names. Open: whether wall tones / pings should follow zoom (by ear), key rebinding.
- In-world windows, first pass 2026-08-22 (survey + exe readouts in `docs/ingame-ui-survey.md`; player keys in
  `docs/controls.md`): the model layer `src/gameapi*.cpp` reads quests, factions, hot slots, lore, bags,
  equipment, skills, masteries and the character sheet through Game.dll exports (`gGameEngine`/`gEngine` ARE
  exported data; `LocalizationManager::Instance()+LocalizeWithoutParams` are public; `GameTextLine` = 0x40
  stride with the u16 string at +8; id -> object via a cached `ObjectManager::GetObjectList` sweep with a
  pre-sized buffer; virtual text builders dispatched through the object's vtable, with COMDAT-folded base
  bodies detected as "ambiguous" slots). Screens (`src/screens/window_base.h`: active = the window's IsVisible,
  Escape = Show(false), tab row + Ctrl+Tab, one column): `inventory` (InGameUI+0xbbf0; Equipment / bags /
  Stats with attribute "+"), `skills` (class selection via `SkillsWindow::SetPane` exe+0x27c580, tree per
  mastery, learn/refund = the window's own IncrementSkillLevel+SubtractSkillPoint sequence, Ctrl+1..0/J/I
  assignment), `codex` (quest tree + lore), `factions`, `vendor`, `stash`, `quest_reward`, `shrine`; in-world
  keys Q objectives, Y quickbar switch, Ctrl+1..0 read slot, G pickup (`InGameUI::HandleKeyAction(0x37)` by RVA, signature-checked).
  VERIFIED through the loop: inventory (equipment, bag read/unequip/re-equip via SmartAutoInsert, sheet,
  attribute point), skills (choose Soldier -> spend on mastery -> learn Cadence -> slot 1 and left mouse),
  codex, factions, Q/Y/G, `/cheat?xp=` (`GameEngine::CharacterExperienceOutbound`) to reach level 2. NOT yet
  seen live: vendor, stash, shrine, quest reward (no NPC of those kinds at the spawn). Lessons: a signature
  longer than the 16-byte check buffer killed the game (exe_ui now clamps); `push rdi` carries a 0x40 REX
  prefix in this exe; `EquipmentCtrl::RemoveItem` only detaches (always AddItem to the bag first); the bag
  map's pair sits at +0x1c/+0x20 (4-byte value alignment), the market map's at +0x20/+0x28. Dev routes:
  `/quests /objectives /factions /hotbar /lore /inv /skills /sheet /obj /loc2 /cheat /ingame?action=`.
- Quickbar + skill targeting (2026-08-23, `docs/skills-targeting.md`, verified live on the test character):
  bare Y is now the game's own Quickbar Switch (passed straight through; `screens::quickbar_tick` watches
  `exe_ui::quickbar_page()` and announces the new bar "quickbar N" -- verified). Reading moved to Ctrl+1..0 =
  read slot 1..10 of the displayed bar, Ctrl+- / Ctrl+= = the left / right mouse skill; each says the skill
  and how it aims. **Aim = `world::skill_aim`**: `SkillActivated::GetTargetType()` returns `*(int*)(this+0x5c0)`
  (never overridden, called directly after an is-a `SkillActivated` guard). The RUNTIME enum is NOT the DBR
  targetingMode order -- read off the game: **1 = self/buff** (Overguard, potions, Field Command), **2 =
  offensive** (Weapon Attack, Cadence, Blitz, Forcewave, War Cry), **3 = ground point** (unconfirmed;
  Evade/Move To are actually 0), **0 = passive/none**. Value 2 is split by the concrete RTTI class name
  (Radius -> "around you" for War Cry, else "at a target"). Verified via `/hotbar` (prints `aim=`) and
  `/action read.leftMouse` -> "left mouse Weapon Attack, at a target". **Input:** `Ctrl+<digit>` is a read
  chord but a digit is also a passthrough key, so `app.cpp`'s game-key filter swallows 0x02..0x0b while Ctrl is
  held (else a real Ctrl+1 reads AND activates slot 1). Synthetic `/key` bypasses that filter and always
  reaches the game, so in-world Ctrl-chords can't be verified through `/key` -- use `/action`. Verified with a
  levelled test char (cheat XP -> learn Cadence/War Cry/Overguard/Forcewave): reads on the displayed page give
  "1 Cadence, at a target" / "2 War Cry, around you" / "3 Overguard, self" and are page-relative (Y switches
  page, reads follow). **Mouse assignability**: the assign API (SetPrimarySlot) accepts ANY learned skill on
  the mouse incl. a self-buff (Overguard -> "left mouse Overguard, self"); the earlier non-stick was UNLEARNED
  skills, not a self-buff rule. Open: confirm real-key Ctrl-chord dispatch on the user's own keyboard (synth
  can't).
- Recover a basic attack (2026-08-23, verified): the inventory Equipment tab is now an `AssignSource` -- with a
  weapon hand slot focused (EquipmentCtrlLocation 9 Right Hand / 10 Left Hand) the assign keys (Ctrl+J / Ctrl+I
  -> left / right mouse, Ctrl+1..0 -> a slot) put the character's **default basic attack** there. The id comes
  from `gameapi::default_skill_id(0)` = `SkillManager::GetDefaultSkillId(DefaultSkill)`, the game's OWN accessor
  (Game.dll export, virtual `[SkillManager+0xe0]`): it SEARCHES the live skill list, so the default-attack id is
  per-character AND weapon-dependent -- NEVER hardcode it (`PlayerHotSlotCtrl::SetToDefaults` calls
  `GetDefaultSkillId(0)` for the left mouse the same way; there is no integer sentinel). Verified: broke the left
  mouse (Overguard), focused Right Hand, assign restored "Weapon Attack" on both buttons; a non-weapon slot says
  "nothing to assign". Deliberate small-scope stand-in for a full hotbar manager. Full reset available via
  `PlayerHotSlotCtrl::SetToDefaults` if wanted later.
- Equip-by-list, hotbar manager, two weapon sets (2026-08-23, `docs/skills-targeting.md`/`controls.md`, all
  verified live on the test char). **Reusable picker** `src/screens/list_picker.{h,cpp}`: a layered overlay
  (layer 30, `open_picker(title, items, on_pick)`); closing re-exposes the launching screen with focus on the
  slot it came from (the existing layered-screen focus restoration -- no `push_child` needed). **Equip picker**:
  Enter on an equipment slot lists every bag item across all bags that fits (`gameapi::can_equip` ->
  `EquipmentCtrl::CanItemBePlaced`), first entry "empty" = unequip; Backspace still unequips directly. Weapons
  go to the ACTIVE weapon set (equip() uses the current EquipmentCtrl). **Hotbar manager** (Ctrl+`,
  `src/screens/hotbar_manager.{h,cpp}`, layer 1 so game windows cover+close it; closes on on_unfocus unless our
  picker is up): lists the current set's two bars (indices 0-9, 14-23), activate a slot -> skill picker (clear
  [id 0 -> `assign_skill_to_slot(idx,0)`] + `gameapi::assignable_skills()` filtered to `level>0` and
  `world::skill_aim != None`, so learned activatable skills only, passives/masteries excluded, item-granted
  included). **Weapon swap = F** (`gameapi::swap_weapon_set` -> `ControllerCharacter::SetAlternateEquipment`;
  the game's "Switch Weapons" is unbound); a weapon set is ONLY the two hands (+0xe0/+0x138), everything else
  shared. The swap propagates to the equipment view a frame later, so the announce ("weapon set N, <right>,
  <left>") is deferred 3 ticks (`weapon_swap_tick` from the in-world tick). Verified: armed set A=Gladius,
  B=Splintered Club independently, F swaps and each keeps its own; equip picker filters correctly; manager
  assigns learned skills + focus returns to the slot. New gameapi: `can_equip`, `swap_weapon_set`,
  `item_skill_ids`, `assignable_skills` (+ exports GetItemSkillList / Controller Get/SetAlternateEquipment).
- Skills: spirit-guide reclaim, requirement-gated learning, modifier targets, stat tooltips (2026-08-24,
  `docs/skills-targeting.md`, all verified live on the test char). **Learning respects requirements**:
  `gameapi::can_learn_skill` replicates the game's SkillReasons gate (exe+0x2492b0) -- points>0, level<max,
  `Skill::GetMasteryLevel >= GetMasteryLevelRequirement`, and a modifier's base skill learned -- and `learn_skill`
  refuses with the spoken reason ("needs mastery N", "requires Cadence", "no points"). **Modifiers name their
  base**: `Skill::GetModifiedSkillId` reads 0 for tree modifiers, so `skills()` reads the sub-skill's own
  `Skill::GetBaseSkills()` into `SkillInfo::modified_skill_id` -> "Discord, modifies Cadence" (2026-09-13: was the
  reverse of `GetModifiers`, which misses the Occultist's `SkillSecondary_PetModifier` skills -- Storm Spirit etc.
  live in `GetSecondarySkills`, so they read as plain skills and the summon's last point could be reclaimed under
  them at a spirit guide; `modifier` = `IsSkillModifier || has a base` (`Skill::IsSecondary` is true on the SUMMON, not its modifiers); the learn gate is now the game's own
  `Skill::IsBaseSkillEnabled`; built, NOT verified live). **Refund only at a
  spirit guide**: talking to an `NpcSkillReallocator` calls `GameEngine::DisplaySkillReallocationWindow`, setting
  the reclaim flag at **skills window +0x1f4c** (the click handler exe+0x248380 reads it as `[controller+0x1e1c]`,
  controller = window+0x130); `exe_ui::skills_reclaim_mode()` reads it. In reclaim mode `screens/skills.cpp` shows
  a hint row + each spent skill's "N iron bits to reclaim" (`SkillManager::GetCurrentSkillReclamationCost`) and
  Backspace reclaims one point; outside a guide Backspace does nothing (fixing the old refund-anywhere bug that
  silently charged bits). `can_reclaim_skill` gives the reason: the mastery bar reclaims down to 1 like any skill
  (base `DecrementSkillLevel`) but its last point is blocked (`tagDecreaseMasteryError`), and a reclaim you can't
  afford (`reclaim_cost() > money()`) says "not enough iron bits". **The game validates reclaims ONLY by greying the
  icon** (2026-08-30, docs/skills-targeting.md): `DecrementSkillLevel` checks nothing and the exe click trusts it, so
  the direct `refund_skill` fallback could orphan modifiers; `can_reclaim_skill` now replicates the whole SkillReasons
  gate (modifiers holding points, a hosted celestial power, a mastery's dependants, bits) and `refund_skill` refuses
  unless it passes. Built, NOT yet verified live. **Stats
  tab**: every row now carries the game's own tooltip on Space (`tagCharAttributeDescription0X`,
  `tagCharStats{OA,DA,DPS}Description`, `tagStatsResistance0XDesc`); attributes remain non-refundable (no reclaim
  path wired). Dev: `/reclaim` opens reclaim mode without a guide; `/cheat?bits=N` tops up iron bits. New exports:
  `Skill::GetMasteryLevel`, `Skill::GetModifiers`, `SkillManager::GetCurrentSkillReclamationCost`,
  `GameEngine::DisplaySkillReallocationWindow`, `Character::AddMoney`.
- Lua (2026-08-22, `docs/lua.md`): LuaJIT 2.0.4 (`x64\lua51.dll`) behind the LUAGLUE binding layer; one
  state owned by `LuaManager` at `*(gEngine+0x68)`; `LuaManager::RunCode` is exported (dev route `/lua?code=`,
  readback via `Game.AddObjective` -> `/objectives`). Sandboxed (no io/os/ffi/debug/require) but `loadfile`
  reaches real paths. It is the quest/level scripting API (tokens, quest state, spawns, doors, teleports,
  banners, XP/items) -- nothing UI-side; everything it does our export calls can do too.
- Rooms (2026-08-22, `docs/rooms.md`, verified through the loop): the game's baked navmesh (Detour tile-cache
  layers inside `world001.map`, 0.25 units/cell, world coordinates; quest gates are runtime obstacles on
  top) is segmented OFFLINE per region (wotr's watershed + an axis-aligned walk-time cap; roads are an
  overlay only) and shipped in `assets/rooms.db` (vendored SQLite, `src/db.*`, read-only). `src/rooms.cpp`
  looks the player up per frame (chunk -> region -> label grid, 8-cell ring, 400 ms dwell) and announces
  place changes in Zira ("Devil's Crossing, room 193"); X = description, V/Shift+V = exits (parks the review
  cursor on the opening via `world::lock_point`). Vocabulary: region = the game's named area, sub-region =
  ours, room = a watershed piece, chunk = the engine's `Region`. Dev: `/room`, `/regions`, `/portals`,
  `/navprobe`. Lessons: `Region::IsUnderground` calls `LoadLevel` (never sweep it); a dev route that outlives
  the 8 s job timeout must own its state (`run_on_game_thread` and `serve()` fixed 2026-08-22). **`World::GetRegionContainingXZ(from, x, z)` takes x,z RELATIVE TO `from`** (world.cpp
  `region_containing_xz` converts; it only looked right in chunk 0A001 whose offset is zero). **Never move the player with
  `Entity::SetCoords`**: it is a raw 0x40-byte field write + `OnMoved()`; the level bookkeeping is in
  `World::SetCoords` and the player's per-frame update (`Engine::UpdateForcedEntitiesInPlayerLoadSphere` ->
  `Entity::Update` -> `ControllerPlayer::Update`) only reaches entities registered in the iterated region, so a
  raw SetCoords stalled the controller (WASD dead, `in_world()` false -> "unsupported screen") and, once the
  stale level was torn down with the controller object, crashed the exe's mouse handler in `SetMoveCommand`.
  `/teleport` uses the game's own `Character::TeleportToLocation(WorldCoords const&)` (Game.dll, exported:
  `ControllerCharacter::Teleport` -> `World::SetCoords` + `NavManager::ResetObject`; axes taken from the
  character), tries rising PutOnFloor start heights and refuses landings the navmesh rejects (verified: 60
  controller ticks/s after same-chunk, cross-chunk and hop-tour teleports). The Lua `Game.TeleportPlayer` is
  the riftgate fade activity (integer coords, async), not a dev teleport. The game pauses single player when it
  believes the window lost focus (a hot reload in the world can leave it paused: `/pause[?set=0|1]`,
  `GAME::Pause/UnpauseGameTime`; `/player` shows `paused=`); `in_world()` no longer needs a live controller. **Devil's Crossing is
  authored** (2026-08-22): 15 sub-regions, 201 of 202 rooms titled + described by `tools/workflows/rooms_author.js`
  (Opus agents over `tools/shots.py` screenshots + `tools/author.py facts`; `docs/rooms-description-rules.md`; a
  mechanical `author.py check`, a sampled reviewer, then a per-sub-region consistency pass -- the per-room
  writers never see their neighbours, so confusable titles and fixtures named from the next room are fixed there;
  ~13M Opus tokens for the region). `devilscrossing:-70:-183` is orphan (its anchor is bake-only; the live
  mesh refuses it). **Hargate's Isle** (chunk 0W021, boat-only via the Row Boat DungeonEntrance) was restored by
  the by-record chunk fix and authored as its own sub-region (14 rooms; 216 total). The dev character dies while posing for screenshots (level 2 among monsters): the shots run
  toggles Lua `ToggleInvincible`, and a shot whose outline is <30 % visible is the tell of a respawn.
  **Lower Crossing + Burial Cave authored** (2026-08-23, covers the first quest): 212 rooms / 12 sub-regions
  (Burial Hill, The Hidden Path and the mires; the sub-region agent reads the game's area names off the
  shots' minimap corner) and the cave as its own region key `burialcave` (`rooms.py area --only --key`;
  25 rooms / 5 sub-regions), all verified; reviewer findings fed the consistency pass as notes. Pipeline
  hardening from those runs (shots.py): approach-then-wait teleports (chunks stream around the PLAYER),
  grid-sampled stepping stones instead of room anchors (the east cliffs are a walkable island 170
  anchor-units off the mainland), `/teleport` calls `Region::BackgroundLoadLevel` for unloaded chunks
  (off-map dungeons like the cave never stream by proximity; no-route targets get force-load + direct
  jump), in-process `gd.capture()`, black-frame retry, open-window guard, `ToggleInvincible(true)` at
  region start (a setter despite the name), flushed per-room timing (~3 s/room). Describers return
  `shots_suspect` and flagged rooms are re-shot + re-described. **Height handling** (2026-08-23, db schema
  v2): stacked walkable layers are tiny in practice (DC 72 m² of ramparts/bridges, the cave one 20 m²
  overpass; the prison's stacked interior is not in the walkable bake at all), so the base plane keeps the
  LOWEST layer + a per-cell floor y (`grids.heights`, RLE int16 decimeters) and the upper layers are sparse
  overlay cells (`grids.overlays`) labeled offline by height-continuity BFS (`gdmap.rooms.resolve_overlays`:
  an overpass joins the room it continues; an enclosed upper floor stays -1 = announce nothing, never the
  room below). Runtime `LabelGrid::label_at(x, z, y)` picks base vs overlay by which floor y is nearer;
  layers closer than 0.9 units are one floor. Verified live in the cave: (1234.9, -469.9) at y 7.7 says
  "stacked coffin floor", the base at -0.5 is "vine curtain gallery"; all 453 authored rooms re-attached
  by anchor on the schema regen (kept=all, orphaned=0). Next: road helpers, the next regions, region names
  from `tagWorldMap*`.
- Runtime exits + a corrected segmenter + a full retag (2026-08-24, `docs/rooms.md`; verified live). Root
  cause of the exit bugs (the user's report: a false cobbled-house-yard<->wrecked-house-gap door, the
  islanded riftgate): the segmentation ran on the baked Detour **tile-cache**, which is flat (we took the
  lowest layer), per-tile seam-eroded, and blind to off-mesh links -- so exits were false (2-D adjacency
  across a wall/height), missing (seam erosion islanded rooms; stairs/quest doors are off-mesh, absent from
  the bake), or duplicated. The map holds ONLY the tile-cache (RLTD blobs, no assembled navmesh); the game
  builds the real 3-D navmesh from it at load. Two changes:
  1. **Exits are computed LIVE at runtime, not stored** (`src/rooms.cpp exit_items`, `world::find_path` =
     `Player::FindPath`, dev route `/findpath`; `core/rooms_model LabelGrid::neighbors_within`). On V / room
     change: every OTHER room whose cells fall within `kExitRadius`=28u (a radius) and that the game's own
     pathfinder can reach is an exit, bearing to its nearest cell. Correct for quest state / doors /
     destructibles (`NavBlocker` = `dynamicblocker_invisible`; `FixedItemDoor`; `Destructible` = breakables --
     all detectable via the sphere query + RTTI name, gate `find_path` exactly as they gate the player) and
     height (find_path drops unreachable stacked floors). **Line-of-sight is NOT required** -- the bearing may
     point through a wall at a room reached by going round (decided with the user; same as the old adjacency
     exits). But the **route must be DIRECT** (2026-08-24, on the user's report that "in A, can directly reach
     B" is the rule, not "B is near and reachable"): each reachable candidate's actual navmesh corridor
     (`world::find_path_corridor` = `NavManager::FindPath`, whose 7th arg is a `mem::vector<WorldVec3>` straight
     path -- all four scalar out-params null-checked, Engine+0x10c2c0; dev `/findpath?...&corridor=1`) is
     classified cell-by-cell against the label grid (`core/rooms_model LabelGrid::path_is_direct`, engine-free +
     unit-tested) and DROPPED if it lingers > ~2u inside any room that is neither the current room nor the
     candidate (a third room = that room's exit, not ours). Off-grid/unlabelled samples don't count as a detour.
     **Strictly additive**: dropped only on a positive detour finding; if the corridor can't be computed the
     candidate is kept (reachability already passed), so a hiccup never loses a real exit. Cross-region
     (foreign) exits still come from the stored `seams` rows (the far room is in another region's grid). The
     offline `exits_of`/`fill_label_seams`/`exits` passes are now vestigial for intra (the mod ignores stored
     intra rows); `seams --write` is still required after any `area --write`.
  2. **Seam-stitched segmenter** (`gdmap.rooms.bridge_walk_seams`, run in `tools/rooms.py build_area`):
     stitches walkable cells across internal tile seams so segmentation matches runtime connectivity, healing
     the riftgate + ~10 island rooms per region. Validated cell-by-cell vs the live navmesh (every bridged
     courtyard cell was on the runtime path mesh; `max_gap`=6, height <=1.5u).
  **Full retag from scratch** on the new segmenter (~30M Opus tokens, ~1.5 h): resegment (`area --write`) ->
  delete dead rooms (anchors moved, keys not in the grid's label_keys) -> re-shoot -> `rooms_author.js`
  (subregions + describe + consistency) per region. DC 195/197 titled (2 untitled = genuinely-unreachable
  off-mesh cliffs), LC 198/198, Burial Cave 22/22; 417 rooms, then `seams --write`. Not fixed (deferred,
  perceived-place segmentation quality, not a bug): boundaries at watershed narrowings through open ground can
  feel arbitrary (cobbled<->wrecked is a real narrowing). Note: the prison courtyard has stacked floors
  (~y3.6 under ~y7); dev `/teleport` floors from the char's incoming y and can land on the lower one (looks
  exitless) -- a tool artifact, the rooms are keyed to the y7 floor the player walks. The mod loads `rooms.db`
  from next to the DLL (`build/ninja/assets/`, copied at build time); a hot copy + `/room?reload=1` tests a db
  edit without a rebuild.
- Painted area names (2026-08-31, `docs/rooms.md` "Painted area names", verified live): the HUD's area name is a
  painted per-cell "sector" layer in each chunk body (GUID tables + a w*h*ntab id grid; table 1 = areas, resolved
  through the map header's unique-entities table + Text_EN) -- decoded offline in `tools/gdmap/sectors.py`,
  validated 100 % against 643 live `Engine::GetAreaNameTag` reads. The location-record regions span several named
  areas (devilscrossing is 60 % "Lower Crossing"), so the spoken place is now **"painted area, sub-region, room"**:
  `rooms.area_name` (schema v3, `rooms.py areas --write`, 10,382 rooms) leads the line and labels foreign exits;
  region name = riftgate zone name, fallback only. Placeholder regions named (c01a = Old Grove, a03a/a04a =
  Prospect Hill -- both CUT content, like the whole location-less 0C/0E corner; map01_gatex01a = Obsidian Throne).
  The whole base campaign is segmented (84 regions / 10,503 rooms, all acts -- "Act 1" in older notes was wrong);
  untagged: the Void (Bastion of Chaos, ~169k m², optional endgame) and the cut corners, all location-less chunks
  the `build` driver cannot see. `/player` prints `area=`.
- Riftgate travel screen (2026-08-22, verified live through the loop incl. a real trip Lower Crossing -> Devil's
  Crossing): `screens/riftgate.cpp` over the world map in riftgate mode (`exe_ui::riftgate_*`, layout in
  docs/exe-ui-layout.md "Riftgate travel"). Review groups: N = people + non-loot objects of interest, M = loot.
  Sonar field (`src/sonar.cpp` + `src/core/sonar_field.{h,cpp}`, 2026-08-24, replaces wotr's left-to-right
  sweep -- which read as noise with GD's fixed viewpoint and fast enemies): enemies / loot / dungeon entrances
  each REPEAT their own cue, the period shrinking as the thing nears (logarithmic in distance, so nearing
  matters most up close: 0.14s@2u..0.80s@25u, the far end 2x the old 0.40s sweep cadence) and their left/right position offsetting the phase (50 % from the
  left = half a period late) so co-distant things stagger instead of merging; rear high shelf on every spatial
  cue; `/sonar` (knobs `pnear pfar dnear dfar radius vol ref floor force`). **One distance curve for every
  positioned cue** (`world::ear_frame`: ref/(ref+dist), ref 10 units, floor 0.2; `/sonar?ref=&floor=` tunes it
  for the review pings too) with only a channel volume per system (sonar `vol=`, 1.0) -- so distance is doubly
  encoded, closer = faster AND louder. Labels: every Actor through the virtual GetGameDescription.
  Corpses are not enemies (`Character::IsAlive`). Main-menu character selection is a middle Tab stop of the
  main menu (the exe's CharacterPicker, docs/exe-ui-layout.md). **The virtual cursor parks at the entity's
  bounding-box CENTRE** (`Entity::GetRegionBoundingBox` = {centre, half-extents}, region frame): the fixed +1.0
  lift put the cursor above ground items, the click hit the ground and fired the default attack (2026-08-22;
  verified: a lore note and the training dummy both resolve by id in `HandleActionFromMouse`). Ground EQUIPMENT still
  resolves nothing (sighted players click its floating label): J on a reviewed Item issues
  `ControllerPlayer::ItemAction` after `SetCommandRepeated(false)` (docs/re_pickup.md; verified walking 16 units
  to a note). Same for a reviewed FixedActor of interest (door, ladder, chest, lever, shrine): J issues
  `ControllerPlayer::InteractAction` (2026-08-30, Flooded Cellar: the click at a ladder's floor point resolved id 0
  every time and the player could not leave; built, NOT yet verified live). `/jkey?down=1|0` presses J without the
  game seeing a J key (synthetic /key goes to both).
- Release-prep pass (2026-08-25, all verified live except where noted): (1) **q objectives fixed**
  (`screens/codex.cpp speak_objectives`): the old walk read every tracked quest's every task and surfaced
  their "return to X" turn-ins (the user's "arbitrary / return to xyz" bug). Now prefers
  `GameEngine::GetObjectives` (the game's HUD list, localized) and, when it is empty -- which it is across much
  of the campaign; the old code comment was right, the disasm claim that the HUD renders from it does not hold
  live -- falls back to each tracked incomplete quest's CURRENT step (first not-complete task with an
  unsatisfied objective). Verified: a char with "Something For Nothing" (kills done) reads "Return to Harmond",
  not the whole tree. (2) **Map picker merged to one flat nearest-first list** (`screens/map_markers.cpp`): the
  quest/non-quest tab split was vacuous (`world::map_markers` hardcodes `quest=false`). Verified live (1 of 23).
  (3) **Enter no longer leaks a world click** (`screens/in_game.cpp`): a close-on-pick screen (map, pause
  "Return to Game", conversation close) re-exposed `InGameScreen` with Enter still physically held, and its raw
  poll turned that into a left-click. Fix: `on_focus` latches a left/right mouse key already held on entry and
  swallows it until released once (built; needs a real keyboard to feel). (4) **Component/augment attach**
  (`gameapi_items.cpp is_component/compatible_items/attach_component`, `screens/inventory.cpp`): activating a
  component (records/items/materia = `ItemRelic`; none ship outside that folder) in a bag opens a picker of
  every item it fits via the game's own `Player::GetCompatibleItems` (bags + equipped + stash); picking one
  calls `Character::UseItemOn(player, comp, target, ItemSource, 0,0,false)` -- a pure inventory op, NO
  blacksmith (the NPC path is removal only). Verified live: Chilled Steel 3->2, target's tooltip shows it
  attached. Arg order confirmed. Dev: `/inv?compat=<id>`, `/inv?attach=<id>&target=<id>`. (5) **Evade**
  (docs/evade.md, corrected 2026-08-28): the game option `evadeFollowCursor` ("Evade To Cursor", Gameplay tab,
  default ON) makes `HotSlotOptionEvade::Activate` dash toward the cursor's mouse-repeat point (+0x440) and
  IGNORE WASD entirely; only with it OFF is it WASD direction while moving, facing when standing. The
  2026-08-25 "WASD steers it" note was wrong (it coincided with the locked target). Player-side fix: turn the
  option off; do NOT use movementType 2 (Keyboard Only) -- that also disables the per-frame cursor pick the
  J/I/lock scheme rides on. `Options::SetBool(0x4b,false)` would force it if ever wanted. A diagnostic hook on the exported
  `ControllerPlayer::EvadeAction` forwarder was tried and removed -- it never fired (the exe calls the state's
  `RequestEvadeAction`/`DefaultRequestEvadeAction` directly; that is the seam if we ever force the direction).
- Options from the pause menu (2026-08-25, verified by the user): the exit window's Options Menu builds the SAME
  Options screen class under a host window at `[InGameUI+0x4def8]` (+0x90; details in docs/exe-ui-layout.md),
  never entering app state 5, so `options_screen()` missed it in the world. It now resolves through the host;
  the options screen is layer 20. So `movementType` is set through the mod (pause -> Options -> Controls tab ->
  Movement Type -> Apply). Since 2026-09-19 gdlaunch forces it before every start instead (next bullet); the old "Steam
  cloud sync fights options.txt edits" worry was wrong: the cloud holds only remote/save/.
- Settings for a shipping user (audited 2026-08-25, FORCED since 2026-09-19): `movementType = 1` and `evadeFollowCursor =
  false` in options.txt (line-preserving) plus `alternate_keybindings.txt` REWRITTEN as the game's default keyboard map with
  W/S/A/D on 63..66 (the launcher owns it; the mod's chords assume the defaults) -- gdlaunch does both before every start (`src/core/game_settings.h`, doctests; the game rewrites both files
  with the same values on exit; Steam cloud holds only `remote/save/`, never Settings). Root cause of the "README steps not
  enough" reports: switching Movement Type to Keyboard creates the four move actions UNBOUND and only the Keybinding tab's
  Default binds them (which also resets every other binding; the dev machine's F on action 50 was lost that way). Plus keep
  default keybindings and `displayDamage` on.
  inactiveUpdateRate / windowed / 1600x900 / targetLock are dev-loop artifacts, irrelevant to a focused player.
  Game-free CI is feasible (nothing links a game file; `gd_names.h`, `rooms.db` and the prism SDK parts the build
  uses are all committed -- a fresh export of the tree builds with only VS 2022, verified 2026-08-25).
  Runnable-by-others gaps: a non-CLI injector/launcher (the dev HTTP server is off by default since 2026-09-18).
- Game patches (quantified 2026-08-25): exports (367, by decorated name) survive a rebuild unless a signature
  changes and degrade per feature; Engine/Game object offsets (~30) survive unless the class changed and fail
  SILENTLY; the exe layer (19 RVAs + ~75 offsets in `exe_ui`, 14 byte signatures checked by `available()`) dies
  on ANY relink of the exe -- every menu/window, deterministically, with the one "version not supported" line,
  the export-driven world layer keeps running. Plan when the first patch lands (deferred deliberately; it needs
  old + new images to be built against): an offline relocation tool (match the 14 signatures + the vtable
  ctors in the new dump, emit a version-keyed RVA table) and vtable validation at each window offset. The
  ground truth for that is the archive: `1.3.0.8-6a85fbec` is in `../grim-dawn-archive` (exe PE timestamp
  0x6a85fbec, the exe's version resource is meaningless). Procedure on a patch: the menu layer dies -> dump the
  new exe -> relocate against the previous build's archive -> when the mod works again, archive the new build.
  Nothing has to happen before Steam updates; only never skip archiving a build the mod works on. Since 2026-09-18 an
  unknown build is refused outright by the version gate (above), so a patch is "the mod is off" for players, not a crash.
- Destructibles on B (2026-08-25, verified live in Devil's Crossing): the B group is flavour NPCs OR an unbroken,
  targetable `Destructible` (`Destructible::GetStaticClassInfo` is-a, `IsBroken()` = +0x66c, `IsTargetable()` =
  the record's `targetable` flag at +0x765 and not broken). `targetable = False` scenery (rustic chairs, tables,
  fences) is excluded by the game's own flag; lootchests/breakables + quest urns are True and label through
  the Actor `GetGameDescription` slot ("Barrel", "Jug", "Crate", "Wooden Door"). Holding J on the lock smashed a
  barrel and it left the list. No sound for them yet. Dev: `/obj?find=<record substring>&max=` lists objects
  by record path with positions (objects in unloaded chunks print "no position"). Dev-driving J: a synthetic
  `/keydown?name=j` ALSO reaches the game (opens Factions); hold Enter instead (`/keydown?name=enter`, the
  other left-button key); `/jkey` is a one-shot click because the in-game screen re-asserts the hold from the
  key poll each frame.
- Exits on stacked floors (2026-08-25, verified live in the prison cellar): `NavManager::FindPath` SNAPS the target
  to the nearest polygon within its radius (our `kNavSnapRadius` 4) and reports a COMPLETE path to that -- a cell
  on the tier above resolves to the wall's foot beneath it, so "reachable" was true for the wrong point (the
  trailing `bool` is accept-partial; we pass false, so partial paths already fail). Gate: the reached point must be
  within `kExitFloorTol` 1.5 u of the destination cell's stored floor (`LabelGrid::floor_y_at`, base/overlay),
  AND (2026-08-30, Flooded Cellar "ruined camp cavern 6 away" across a drop-off: result 0, endpoint 4.9 u short on
  our own ledge, same height, corridor uncomputable) within `kReachTol` 1.5 u of the destination cell or inside the
  neighbour's label -- a snapped endpoint on our side is not a route. Built, NOT yet verified live.
  Exit positions are the corridor's entry point into the room (`path_entry_point`); exits never auto re-ping.
  Cross-floor enemies in the review/sonar groups stay flat (the user: not a problem in practice). `/los?id=` casts
  the exe's cursor-pick ray (image points are viewport FRACTIONS): raw LOS is stricter than a sighted player's
  view because the renderer cuts away the covering tier -- "on screen" remains the visibility rule.
  **TRAP: never `class_name()`/`rtti_of()` a `Region*`** -- the cached `Object::GetRTTIClassInfo` slot is 0, which
  on a Region's vtable is the virtual destructor; it destroyed the live region and crashed the render.
- Partial-stack selling (2026-08-26, verified live at Kerrick with a Serrated Spike stack): Ctrl+Enter on a stack
  in the vendor's Sell tab opens the mod-owned count prompt (`screens/count_prompt.{h,cpp}`: a layer-30 raw-input
  overlay like the list picker -- digits by scancode, Backspace, Enter commits 1..max, Escape cancels; reusable
  for the stash later). The split is the game's own stack-split OK sequence minus the cursor
  (`gameapi::split_stack` / `sell_split` / `unsplit_stack`, details in docs/ingame-ui-survey.md "Stack split").
  **Never `PlayerInventoryCtrl::AddItem` a split clone**: the grid add merges it back into its source stack and
  destroys it. The only non-export constant is the inline `ItemReplicaInfo` at Item+0x538 (guarded: its first
  dword must equal the item id). Sighted players split with Ctrl+click (dialog) / Shift+click (half) onto the
  cursor; consumption-side actions (crafting, attaching, potions, turn-ins) draw from stacks themselves, so the
  only other place a split is wanted is the stash (not wired).
- Type-ahead fixes (2026-08-26, the user's report: arrowing through results jumped across tabs, and letters
  fired world keys): (1) `GraphNavigator::search_nodes_` held `GraphNode*` across frames (the classic trap);
  now `{ControlId, text}` resolved in the current render (regression test in tests/navigator_tests.cpp).
  (2) `ScreenManager::live_categories` stops at the first screen that OWNS the keyboard, not only at an
  `exclusive()` modal -- a window over the world no longer leaves the world's `InGame` letter keys (. , N B M V
  F G Q K X H T) live under type-ahead. The game's Ctrl-lifts (`game.*`) moved to a new `InputCategory::Lifted`
  declared by the in-game screen AND `WindowScreen`, so Ctrl+N/Ctrl+M/... still work from inside a window.
  Verified through the loop (dev `/key` letters ALSO reach the game -- the map/codex opening on m/q in a
  dev run is that artifact, not a filter hole; the user is checking real keys).
- Pets mapped (2026-08-26, `docs/pets.md`, RE in `docs/re_pets_gamedll.md` / `re_pets_exe.md`; verified live on the
  test char with a raven + hellhound): pets = `GameEngine::GetLocalPetList` (uncapped; the HUD portrait order = the
  F2-F6 order); label/life through the usual Character paths; the summoning skill via `Character::GetPetPen` +
  `PetPen::GetPetOwner` (returns a SKILL id); **stance is per summoning skill** (`Player::Get/SetPetControllerType`
  keyed by skill id, 0 normal / 1 aggressive (default) / 2 defensive, applied to live pets with
  `Monster::UseController`); disband = `ControllerPlayer::ReleasePet(id,false)`; per-pet attack =
  `Character::RequestAttack(pet, playerId, target)` (works); `Character::RequestMove` behaved as a recall only.
  The game's F2-F7 selection is exe-side (`X+0x858` list, X=`[main_obj+0x90]`) and makes the NEXT world click a
  pet command (not reproduced through the dev loop). "Pet Attack" (default skill 7619) works from the hotbar key
  with the review lock; `activate_hotslot` alone does not; `skill_aim` says None for it (hotbar manager hides it).
  `src/gameapi_pets.cpp`, dev `/pets`. BUILT the same day (`src/screens/pets.{h,cpp}`, verified through the loop):
  `]`/`[` cycle pets (`ScanGroup::Pets`, stance as the note), Backspace = pet overlay (stance Left/Right, Enter
  select, Backspace disband, Space where, attack-locked-target / recall rows), F2-F6/F7 = OUR selection toggles
  (the game's swallowed), Shift+Backspace = attack from the world, "<pet> summoned/down" in Zira from list
  polling; Pet Attack listed by the hotbar manager. Decided: no pet health (nothing heals them).
- Devotion BUILT (2026-08-27, `docs/devotion.md` section 4, verified through the loop): the skills window gets
  Constellations (tree per constellation, stars BFS from the root, Enter spends, Space tooltips) and Celestial
  Powers (host picker) tabs, affinities on the sheet; `src/gameapi_devotion.cpp`, `exe_ui::devotion_constellations`.
  Skills window: spends now go through the pane's own icon click (`exe_ui::skills_press_skill`) and the game's
  **Undo Points** button is a row (`skills_undo_points`); reclaim flag corrected to the per-pane byte. Devotion
  reclaim at a spirit guide: Backspace on a star (`reclaim_star`, gates `can_reclaim_star`), verified live. **Affinity
  is not saved by the game** -- it is derived from complete constellations when the map is shown; `constellations()`
  reconciles the counters itself (a loaded character read affinity 0 with a learned Crossroads until then). Lesson: a
  by-value `std::string` argument is destroyed by the CALLEE (MSVC x64) -- never free it after the call. Known
  pre-existing quirk: switching tabs while focused on a row that vanishes speaks a stray "<tab>, N of N" landing
  (inventory too). The mapping notes (static RE): stars are `Skill`s (`GetSkillOperation` 2 = star, 3 = celestial power), the exe holds the whole
  constellation graph at `devotion window+0xa8`, spending/reclaiming are immediate export calls (no command),
  `Skill::GetDevotionParent` = the HOST skill a power is bound to, binding = `host->SetAutocastSkill(power,
  power->GetTemplateAutoCast(), false)` + `SetDevotionParent`, reclaim mode is opened by a SPIRIT GUIDE's
  Devotion tab as well as a Tonic of Clarity (verified live: `GetUI()->vt[0x90](id)` sets `window+0x2419`; the
  wiki was right, the exe note's "tonic only" was wrong), AffinityType 0..4 = Ascendant/Chaos/Eldritch/Order/Primordial.
- Desecrated shrines (2026-08-28, verified live at the Flooded Passage shrine): the "summon what is trapped within"
  shrine is a SECOND window of the shrine shape at InGameUI+0x7f6f8 (own class, vt exe+0x318128; same widget
  offsets as the ruined shrine's +0x7da50), so `screens/modals.cpp` now runs the ShrineScreen over both offsets.
  Start = the window's own button; the game also opens the inventory next to either shrine window (why the bug
  looked like "the wrong window"). The shrine title/info text elements are two more framework-B text classes
  (`kTitleTextB` u16 +0x40, `kTextBlockB` u16 +0x38) -- `WidgetB::text` reads them now (the ruined shrine's
  title/info were silently empty before). Dev: `/shrine` prints both windows; `/ingame` lists them.
  Finding an in-world window's offset: scan the live InGameUI for a vtable with `/peek` (the map header's
  per-region shrine record + `tools/gdmap` locate the shrine; the chunk needs `/teleport?check=1` polling to load).
- Sonar interactables (2026-08-28): `ScanGroup::Interactables` = the N group (quest NPCs, merchants, objects of interest)
  minus DungeonEntrance / StaticShrine, which have their own cues, ping on
  `assets/audio/interactables/interactable.wav` (se_old_pack00 buble05, copied into the repo; trim +4.7 dB by
  tools/loudness.py). Verified live: the character's grave marker pings. Sound files are copied into the repo, never
  referenced from outside it.
- Loot filter MAPPED + BUILT (2026-08-29, `docs/loot-filter.md`, verified live): Ctrl+O = the window as four Tab stops
  (column header, toggles in the game's order, "set to defaults" per column; Enter = `Player::SetLootFilter` + a mirror
  write of the box, Space = the `tagLootFilterNNInfo` tooltip); bare O = "show all items" latch (groups + sonar ignore the
  filter, the actor capture's Alt byte `[[main_obj+0x90]+0x110]+0x129` re-asserted per frame). `world::scan` drops ground
  Items failing `Item::PassLootFilter(0)` and EVERY entity with `Entity::GetVisibility()==0` -- the "Strange Key you already
  have" ghost: a placed quest item is hidden by `QuestItem::InitialUpdate` -> `SetVisibility(GetQuestVisibility())` once its
  `requiredTaskUID` task is done, but `IsOfInterest` still says yes (not yet seen live on a real key). Model facts: 42 `LootFilterOption` bits per character at
  `Player+0x4c00` (`Player::Get/SetLootFilter`, `SetLootFilterDefaults`, no range check, saved in the .gdc); the
  window `InGameUI+0xab410` holds a `std::map<CheckBox*, option>` at `+0xd58` (checked byte `+0x282`, caption tag
  `+0x338`; 41 boxes, option 39 = expansion 3 only) and calls SetLootFilter on each click, so a screen is
  `SetLootFilter` + a mirror write of `+0x282`. Full index -> tag -> semantics table in the doc, verified live by
  walking the map. Only the Pickup key (G) and the label pass honour the filter; J on a reviewed item and our review
  groups ignore it (product decision open).
- Blacksmith / crafting window MAPPED + BUILT (2026-08-29, `docs/crafting.md`, `screens/crafting.cpp`, a ribbon crafted live; RE in `docs/re_crafting_{exe,gamedll}.md`;
  verified live with a Lua-spawned Angrim at the Outskirts camp): `InGameUI+0x3aa80` (vt exe+0x31cf38) is a frame around a
  by-value crafting panel at `+0x1e40` (rows vector `+0x3070` stride 0xd0: text, selected `+0x58`, formula id `+0x64`;
  Combine enabled iff `+0x2129==0`; select via the list box exe+0x1f9f00). Model is all exports: formulas are
  `ItemArtifactFormula` items (`GetPlayerFormulas` + `NpcCrafter::GetRecipes` + defaults), `GetReagentNCount/QuantityForFormula`,
  `GetCreationCost`, `GetMaximumCraftable` (the "[N]"), `GetArtifact()` = the unrolled template result (ranged tooltip).
  `SendCreateArtifactCmd(CreateArtifactConfigInfo)` VALIDATES NOTHING -- press the window's Combine, never call it.
  The only blacksmith window; dismantle/salvage are the Inventor's (below); "transmute" is the Illusionist (unmapped).
- Inventor window MAPPED + BUILT (2026-08-30, `docs/inventor.md`, RE `docs/re_inventor_exe.md`, `screens/inventor.cpp`;
  verified live with a Lua-spawned Darlet: Keep Add-on, Keep Item answered No, Dismantle with a component): the exe's
  "enchanter" window `InGameUI+0x30dd8` (vt exe+0x31d050), tab index `+0x8bc0`, tab buttons `+0x8c08/+0x8f40/+0x9278/
  +0x95b0` through the radio registry `+0x8bc8`; Salvage panel `+0xa38` (chamber `+0x40`, Keep Add-on `+0x2b0`, Keep
  Item `+0x660`, Remove Augment `+0xa10`, registry `+0x268`, cost text `+0xdc0`, dialog pending `+0x10a8`), Dismantle
  panel `+0x1af8` (chamber `+0x108`, results `+0x330/+0x558`, button `+0x7c8`, registry `+0x780`, pending `+0x1148`).
  Convert / Reroll are expansion tabs (greyed by `MainPlayerCanUseConvert/Reroll`; base install has no records for
  them) -- listed only when enabled, unmapped. **The chamber owns its item**: the drop is box `SetItem` (vt+0xa8) +
  `PlayerInventoryCtrl::RemoveItem`, the return `ControllerPlayer::GiveItemToPlayer`; results (the kept component /
  item, scrap + bonus) come back INTO the boxes and must be taken out -- the screen does that and names them. The
  screen is item-first (rows = eligible bag items with the computed price; Enter = action picker / dismantle; the
  game's confirm box is answered through message_box; `on_update` reconciles done / cancelled). Prices are computed
  (`0.05 x GetItemCost`, `itemLevel*10+150` + component salvage) and matched the panel's own texts live. Dismantle
  needs the `DISMANTLING_UNLOCKED` token (Lua `GiveToken` for tests) and a re-open of the window. Dev: `/inventor`.
  Lesson: `WindowScreen::add_tabs` with an empty label list throws inside `GraphBuilder::end_row` and takes the game
  down with the mod's exception -- never call it with no tabs.
- Enemy attack telegraphs MAPPED (2026-09-01, `docs/telegraphs.md`, verified live): the game has none; the sighted
  player reads the wind-up animation. Pipeline `SkillActivated::StartAction` -> animation callbacks
  (`SkillManager::HandleSkillAnimationCallback`, `GAME::Name` = 32-bit FNV-1a of the string) -> `HitAction` (the
  concrete class's `ActivateNow` does the geometry at the hit frame) -> `EndAction`. `src/casts.cpp` + `/casts` log it
  (dev only). Windows: enemy melee median ~0.6 s, specials ~0.8 s, ranged basics 0.2-0.3 s, up to 2.9 s; the .anm
  trailers in Creatures.arc carry the hit frames offline. **Hook trap**: the base `SkillActivated::HitAction/ActivateNow/
  StartAction` exports are folded stubs shared by ~2000 exports (detouring one killed the game silently at start);
  `hooks.cpp` now refuses bare stubs + duplicate targets. Shape taxonomy = the skill object's RTTI class (wave, radius,
  projectile/burst/ring, area pool, charge, tendril, summon, buff) + the record's geometry fields.
  BUILT the same day: `src/telegraph.cpp` plays one of five 100 ms Zira-word cues (swing / stomp / wave / shot / ring =
  the five reactions; `assets/audio/telegraphs`, `tools/gen_telegraph_cues.py`) positioned at a hostile caster at
  StartAction; a weapon attack started from > 4.5 u counts as a shot. Verified firing through the loop; NOT yet heard by
  the user (ramp-to-hit and the inside-the-shape test are the next steps). Dev: `/telegraph`.
  **T = announcement toggles** (2026-09-01, `screens/announcements.cpp`, two Tab stops "announcement settings" / "telegraph filter",
  push_context titles): outgoing off / brief (hit, crit, miss, blocked) / full (Mark numbers; kills+XP in both) / incoming
  (Zira health steps, effects on you) / incoming hits ("hit" per attack reaching you via `CombatManager::TakeAttack`,
  works while invincible) / telegraph cues as a FOUR-STATE (off, your target = reviewed or combat
  enemy, highest tier = casters at the top MonsterClassification within 25 u, all) + a per-shape on/off stop; persisted in `%LOCALAPPDATA%\Grimdark\settings.txt` (`src/settings.cpp`,
  key=value; the mod's only persisted settings). The authoring note that was on T is the dev route `/note`. Cue files are
  loudness-matched to the sonar's enemy cue (K-weighted -13.9 LKFS, limiter) and play above the master on the voice rolloff.
- Phantom mouse hold mutes WASD (2026-09-01, diagnosed live, fix built, NOT yet verified): the exe keeps its own
  "button held" bytes at world screen `[main_obj+0x90]+0x88` (left) / `+0x89` (right); while either is set its per-frame
  WASD routine (exe+0x2c2b5, gated after `GameEngine::GetInputMode`==0 and movementType) issues NO
  `HandleActionFromJoystick` (a held button's repeat outranks the keyboard). Its mouse handler ignores events outside
  the client area, so an injected transition delivered off-window is lost and the byte sticks. Seen: J held on a locked
  enemy that died -> the release fell back to the REAL cursor (the user's mouse sits at client (-86,193)) -> WASD dead,
  no window open, nothing paused. Fix: `world::mouse_key` presses only where there is an on-screen point (virtual cursor
  on screen, else the real cursor inside the window; otherwise "too far away" / "no target"), and releases at the virtual
  cursor or, when its target left the window, where the player-to-target line leaves it (`core/screen_clip.h`,
  unit-tested; margin 4 px); `hooks::set_mouse_hold` now takes the position. Recovery without the fix: lock anything on
  screen and tap J once. Dev: `/peek?ptr=<world_screen+0x80>` shows the bytes (`/ui` prints `world_screen=`).
- Inventor confirm timing (2026-09-04, built, NOT verified live): after Yes the exe clears the chamber and sends the
  command on its NEXT update, so "item still in the chamber the frame the box closed" is not a No. `screens/inventor.cpp`
  waits (Yes known via `exe_ui::last_dialog_answer()`, else a 20-frame settle) instead of speaking "cancelled" and handing
  the item back mid-command (docs/inventor.md "Timing lesson"). Same-day crash triage (exit to main menu after a rift trip,
  once): the exe's InGameUI destructor hit `Paperdoll::Destroy` on the Illusionist window's private `__PaperDollRegion`
  whose Level had been `UnloadEntities`-torn down (null bucket vector; the 4 other paperdoll levels intact). The only
  callers of that teardown are engine-internal (`Region::UnloadLevel` <- `World::UpdateRegionUsage`, `RebuildMapData`,
  `Level::CreatePathMesh`); neither the exe nor the mod imports them -- treated as a game bug. Crash forensics recipe:
  `gd.py status` (now survives the dead health probe), `stacks.py`, the crash reporter's `%LOCALAPPDATA%\Temp\<guid>\minidump.dmp`,
  and reading objects out of the still-alive crashed process (return-slot scan of the crashing thread's stack).
  **Enter on a bag item no longer reaches `PlayerInventoryCtrl::UseItem` unless the item is-a OneShot, ItemNote, ItemArtifactFormula
  (blueprint), ItemFactionBooster/Warrant, ItemDifficultyUnlock or ItemDevotionReset** (the first cut, potions+notes only, refused a
  blueprint live 2026-09-04 -- "not usable" on Guardsman's Defender; augments (ItemEnchantment) now take the component picker)
  (`gameapi::is_usable`; the screen says "not usable"): UseItem's non-potion branch (Game.dll+0x3e00c0) removes the item
  from the bag on the item's own virtual "use" before the command runs, and a crafting material (Class QuestItem, Royal
  Jelly) vanished that way live. Rule for both fixes: never hand an item to a game control the exe itself gates by
  class/state; mirror the exe's gate first. Item-moving calls all live in `gameapi_items.cpp` (RemoveItem/AddItem/
  GiveItemToPlayer/SmartAutoInsert/UseItem/UseItemOn/SendDropItemRandom/split) + `exe_ui.cpp` inventor_put/take.
- Riftgate travel reworked + faction vendor tiers + compare fallback (2026-09-04, built + link-checked, NOT verified live;
  the game was running under the user). Riftgate screen: Tab stops "all" (personal rift, Devil's Crossing, then nearest
  first) and one per act (the master table `riftgate_mastertable.dbr Region001ZoneList` is a flat list authored in
  progression order; act = the zone record's chunk letter 1a/1b/1f/1g -> `tools/gen_riftgate_zones.py` ->
  `src/riftgate_zones.h`, icons matched by localized ZoneNameTag). The Ctrl+M map screen gained a "shrines" Tab stop (`screens/shrine_list.h`): the 28 campaign devotion shrines
  come from world001.map's per-chunk shrine record (`tools/gen_shrines.py` -> `src/shrine_table.h`: icon record,
  FileDescription area, zone tag, chunk origin as the position, corrupted/ruined flags, chunk GUID) and are shown when
  `Player::GetDiscoveredShrineUIDs` / `GetShrineUIDs` (restored) contain the chunk GUID -- **that UID = chunk GUID
  identity is a hypothesis; `/shrines` prints both sides to confirm**. The map's "sections" are 512-px image tiles,
  not categories; the game names acts only in achievement groups. Faction vendor: same window class as the market
  (`+0x2e188`) with tabs 1-4 = reputation tiers (tagFactionVendorTab01A..04A = Friendly/Respected/Honored/Revered,
  tab 5 = Buyback); `exe_ui::vendor_tabs` reads the window's tab map (+0x26f8: node+0x20 Market_TypeEnum, +0x28 tab
  button; +0x25e0 selected type; the exe disables a tab (+0x281) when its sack is empty), the lock is computed as
  faction value < `GameEngine::GetFactionLevelValue(tagFactionStateFriend1/2/4/5)` with the merchant's faction from
  `Character::GetVisibleFaction(market id object)`; the game itself only shows lock art + a red "Insufficient Faction
  Status" tooltip line + tagMarketError05 on purchase. `/vendor` dumps both windows' tab maps. Backslash compare falls
  back to `gameapi::equip_slots_by_class` (RTTI class -> EquipmentCtrlLocation) when `CanItemBePlaced` refuses every
  slot (requirements not met). Stash status: the caravan screen (`StashScreen` in vendor.cpp) lists stash + transfer
  sacks and moves stash -> bag; `gameapi::bag_to_stash` exists but nothing calls it; no remote stash in the game's UI,
  but `Player::AddItemToPrivateStash(sack, itemId, bool)` + `PlayerInventoryCtrl::RemoveItem` +
  `ControllerCharacter::SendRemoveItemFromInventory` is the game's own cursor quick-drop sequence and needs no window
  (private stash = Player data saved in the .gdc; the transfer stash needs `GameEngine::SaveTransferStash`).
- N and the sonar agree on entrances (2026-09-04, diagnosed live in Hanneffy Mine, built, not yet reloaded): the one-way
  exit shaft the player came in through (`ugdoor_caveexitonewaya01_glowshaft`, `locked = True`, no description) is a
  `DungeonEntrance` whose `IsOfInterest()` is false, so the N group (objects of interest) skipped it while the sonar's
  transition cue (every DungeonEntrance) pinged it. Now every DungeonEntrance is in N; an unnamed one reads "entrance"
  and a not-of-interest one carries the note "locked". `/entities` marks `[FixedActor no-interest]`; `/sonar` lists
  what is pinging. The dev server serves ONE request at a time (parallel gd.py calls fail) and git bash rewrites
  `/player`-style paths unless `MSYS2_ARG_CONV_EXCL="*"` is set.
- Stash redesigned + two tab-row fixes (2026-09-04, built + link-checked, NOT verified live). `StashScreen` (vendor.cpp):
  tabs your stash / shared stash (stops: your items -> `gameapi::bag_to_stash_any` = the game's shift-click into the
  selected sack first then any sack with room; in the stash -> `stash_to_bag`) / manage stash (`exe_ui::caravan_panel`:
  panels at caravan+0x13c8 private, +0x13d0 transfer, cost vector<int> at panel+0x378 = InventoryCostArray /
  TransferPageCostArray, self-checked as [0, rising]; buying = `gameapi::buy_stash_sack` (SubtractMoney + Player::AddSack
  / GameEngine::AddTransferSack, the exe's handler exe+0x131150/+0x1316d0 minus its UI) then `exe_ui::caravan_refresh` =
  the exe's own tab-list rebuild exe+0x25d890(panel+0x98, sack vector) + sack dims exe+0x12ec70, signature-checked).
  Base game: 5 private tabs (0/25k/50k/100k/200k) and 5 transfer tabs (0/50k/100k/150k/250k), 10x19 cells each, rectangles
  only. Tab fixes in `WindowScreen`: `on_push` resets to the first tab (the remembered tab of the last visit was announced
  while the game reopened on its first), and `select_tab` with speech now lands focus on the new tab's node (Ctrl+Tab from
  inside a page lost its row on the rebuild and fell to the stop landing = tab 0's node, misreporting the tab).
- Virtual cursor vs the HUD (2026-09-04, question from the user, built, not yet reloaded): YES, a J/I press whose projected
  point lies on the HUD clicked the HUD -- the root mouse handler exe+0xbef10 hands every event to the UI child first
  ([root+0x2b8]->HandleMouseEvent) and the world only sees unconsumed events; press_point accepted any point inside the
  client rect, and the HUD block (toolbar + portrait/status + compass strip, 1024*scale wide, bottom-centre) is
  x 398..1203 / y 781..900 at 1600x900 -- a near target south of the player projects there. Fix: `exe_ui::hud_rects`
  reads the rects InGameUI::Init copies out of its positioning windows (InGameUI+0xb748 toolbar, +0xb758 status, +0xb768
  block top-left; framework-B windows hit-test `this+0x40/+0x44` + `(this+0xa0)->vt[0x98]()` = {x,y,w,h}); `press_point`
  refuses such a point ("behind the interface") and `release_point` moves a release off the HUD (a swallowed release
  is the phantom-hold bug again). Not covered: the floating minimap corner. Dev: `/hud[?x=&y=]`. The world screen's
  byte +0x121 is a per-frame "click handled" latch, not a hover flag; the game exposes no mouse-over-UI predicate.
- Aim by direction + Toggle UI (2026-09-17, aim built + compiled, NOT verified live; toggle announcement built): the
  game's Toggle UI (`]`, key action 0x3c) flips InGameUI+0xac990 (ctor 1 = shown), read ONLY by the UI render pass --
  VERIFIED live: the hidden HUD still sets rollover, still opens the exit window on a click, no world click; so the
  "behind the interface" refusal cannot be dodged by hiding. `exe_ui::ui_visible`, spoken as "interface shown/hidden"
  from `quickbar_tick`. Measured at max zoom (player at screen centre, pitch 46, camera to the south): window edge
  ~21 u north, ~22 u east/west, ~13 u south; the HUD block starts 10.3 u south within ~11 u east-west. A sighted
  player never clicks a distant enemy's body: every skill but the charges fires at the cursor's ground point (enemy
  search near it, else the point) and they rotate the camera. So `world::press_point` now aims a locked target whose
  point is on the HUD or off the window BY DIRECTION (`aim_along_line`: screen ray through a point 6 u out, clipped to
  the window, backed off the HUD by `core::back_off_rects`); "too far away" no longer fires for a locked target.
- Row-vanish landing + announcement fixed in the core (2026-09-04, from the stash screen: Enter on the first item bounced
  to the tab strip and every Enter repeated the list title; 2 new doctests): (1) `GraphBuilder::add_item` stamps an
  actionable node's `land_group` with its Tab stop key when the screen set none -- reconcile's tier-3 rule (nearest
  survivor OF THE SAME GROUP, next first) had never fired because nothing stamped groups, so the fallback walked the
  order backwards and the first row's predecessor is the last tab. (2) the navigator remembers the spoken node's context
  path (`last_spoken_path_`, `GraphAnnouncer::compose_from_path`) so a landing after the spoken node vanished reads as a
  sibling move, not as an entry from nothing that re-reads the titles.
- Quest reward window FIXED + reward rows (2026-09-06, `docs/ingame-ui-survey.md` "Quest reward", verified live through
  the loop on the dev char: a Bounty Table turn-in opened the screen over the dialog with "Quest Progress, Bounty: Nicholas
  Balthazar, XP: 5000, 800 iron bits, 125 reputation, Devil's Crossing, -50 reputation, Cronley's Gang, Close"; Enter on Close
  handed focus back to the conversation): the window (UIQuestRewardWindow, InGameUI+0x8efd8) never reported through the generic IsVisible slot
  (+0x68) -- it paints itself by setting the base control byte +0x28 from its quest-completed event handler and
  its Close routine clears it -- so `QuestRewardScreen::is_active` now reads +0x28 (`WidgetB::visible`). It opens
  beside the still-open conversation and outlives it, so the screen is layer 32 (conversation 30). Rows = what the
  task handed out, from `src/quest_rewards.cpp`: hooks on `Quest2Task::Complete` (every completion path) and
  `ScriptableActionCollection::Execute` (inside a Complete, read the `IsReward` actions BEFORE they run: amounts by the
  exported `Give*::GetAmount`, items by `GiveItem::GetNumItems/GetInfoItem` -- generated by OnActivate a moment
  earlier -- faction via `GetFactionTag` -> `FactionPack::GetFactionFromString/GetFactionTag` -> localize; class =
  vptr vs exported `vftable' symbols, GiveFaction's is not exported = the leftover). No spoken line of its own
  (decided with the user): the window IS the announcement. `Quest2Event::GetText` is empty in practice. Completions that hand
  out nothing are not recorded: the Bounty Table's "Report in" step completes EVERY other bounty's Turn In task too (21 empty
  `Quest2Task::Complete` calls for one real one) and they pushed the real record out of the ring. Dev: `/rewards`;
  `/quests?complete=<questptr>&task=<i>` = the game's own `Quest2Repository::CompleteQuestTask` (rewards run only when the task
  is IN PROGRESS -- stage it first with Lua `Game.GetLocalPlayer():GrantQuest(questId, taskUid)`; `/quests` prints `uid=`).
- Equip requirements explained (2026-09-11, built, NOT yet reloaded; docs/ingame-ui-survey.md "Requirements"): the sheet
  TRUNCATES attributes (they are fractional floats; the game compares the raw value, so a rounded "392" failed a 392
  requirement), Enter on a bag item you can't wear says which stat is short ("requirements not met, Physique 391 of
  392": `gameapi::requirement_shortfalls`, a replica of `ItemEquipment::AreRequirementsMet` incl. the requirement-
  reduction attributes, SELF-CHECKED against the game's verdict -- nine item offsets are the only non-export facts), and
  an equipped item the game has detached (`EquipmentCtrl::Sift` on any attribute change: stays in the slot, contributes
  nothing) reads "<name>, inactive" via the exported `IsItemAttached(id)`.
- **The game's north is NOT the mod's** (2026-09-11, `docs/compass.md`, `tools/compass_fit.py`): the dialogue's compass is
  screen-up at the DEFAULT camera (yaw 0.8727), fitted over 26 text anchors (rms 34 deg; yaw 0 scores 63, yaw pi/2 51). The
  mod keeps yaw 0 deliberately: the world geometry is on the axis-aligned tile grid, so yaw 0 is what makes walls read
  straight and W follow corridors; the default yaw would turn every corridor into a NE/NW diagonal. Player rule (README):
  the game's north = 11 o'clock, east = 1-2, south = 4-5, west = 7. Nothing in the mod's wording should call yaw 0 "north".
- Map icons named as the game names them + widest zoom (2026-09-11, `docs/map-icons.md`, built, NOT yet reloaded): the
  sighted "your objective is that way" is a point-of-interest record (`records/ui/mapaerial/poi`, class AreaOfInterest,
  53 of 100 bound to a quest task and shown only while it is active) on the aerial map, plus the quest log prose; the
  map is an orthographic camera (view height = mapZoom * 3 units, wheel clamp 40..135 -> at most 405 x ~650 units)
  and its icons come from that frustum. `world::map_markers` now reads the nugget's own name (+0x10), the custom symbol
  texture (+0x50, `Resource::GetFileName`, "obstacle") and speaks kinds with the game's `tagMapSymbol*` words;
  `exe_ui::aerial_zoom_set` (MiniMap+0x166c/+0x1670, verified live) holds 135 while the Ctrl+M screen is open.
  Decided with the user: no icon gathering beyond the sighted reach, no offline quest-POI table -- parity first.
- **Painted damage ground = the engine's sector damage layer, unresistable** (2026-09-13, `docs/hazards.md`, verified live in
  the Amalgamation's yard): per-chunk sector table 7 (`DamageSectorData`, defined in the map header: name, amount, type,
  fx) is read once a second by `TickManager::Tick` and applied as amount x max life straight through
  `CombatManager::ApplyDamage` -- no resistance (Aether Act3 Boss 0.12, Aether01 0.15, Aether02 0.30, poisons 0.02..0.20);
  the glowing hotspot decorations are only the picture. `world::hazard_at(point)` is that lookup for any point
  (`Level::GetSectorLayers` -> `SectorLayers::GetTargetId(7, x, z)` region-relative ints -> `SectorDataManager::GetSectorData`
  on `gEngine+8`), `world::mesh_contains` the closest-point containment gate. `src/hazard.cpp` (on by default, `/hazard`
  knobs, `?at=x,z` probe): sizzle lanes on the wall-tone rectangle (nearest lane), a 260 Hz sizzle bed pair while
  standing in it, and a triangle-pulse pointer cycling every reachable safe island (pitch = north/south, pan =
  east/west, order hysteresis). Sounds from `tools/gen_hazard_cues.py`, chosen by ear; sound glossary has the section.
  Skill hazards (traps, the boss's geysers) are the normal, resistable pipeline. "Purity" (`IsPurityActive`) is an unset
  Titan Quest leftover. Heard and tuned by the user in play (bed 0.45, pulse 0.7 at 0.2 s, lanes half-width 0.5).
- **Ctrl+T = sound cue settings** (2026-09-13, `src/cues.{h,cpp}` + `screens/cue_settings.cpp`, modelled on T; built, NOT
  yet verified live): per-cue on/off (wall tones, harmful ground as one, each sonar group -- an off group is not collected)
  and six percent volumes (walls, hazards, enemy pings, other pings, the Mark voice, the Zira voice) multiplied onto the dev knobs, 5 % steps on a -60..0 dB scale (3 dB each, 0 = silence) with a preview of the channel's sound at the new level; persisted as `cue.*` /
  `volume.*` in settings.txt like T's keys. (Ctrl+Backslash was the first key: 1Password's global autofill chord took it before the game.)
- Freed-object crash fixed (2026-09-14, a Discord report from a VM run; verified live: world entered, /casts still
  names classes + records, /entities fine, no stray crash lines): `casts.cpp` stored raw caster/skill pointers in its
  hooks and read their class names in `tick()`, which runs on the next in-world frame -- minutes later under emulation
  (load + intro cutscene), after the game freed them; one call through a reused object's vtable ran garbage,
  corrupted the heap and ntdll fast-failed the process (no crash reporter, the window just vanished). Now every
  object read happens inside the hook (`read_objects`), `Raw::skill` is an opaque key. `world::rtti_of` refuses any
  vtable (or slot) outside the exe/Engine.dll/Game.dll images, which covers every class lookup. `src/crash.cpp`
  = a first-chance vectored exception handler writing code/address/registers + a return-address scan of the stack
  (module+rva, no heap, own file handle) to grimdark.log; ordinary codes are deduped per (code, address) and
  capped at 200, heap corruption / fast-fail / execute faults always logged. `tools/vsdev.cmd` finds any VS 2022
  edition through vswhere (Build Tools included). Note for anyone reading a tester's log: a new character's intro
  cutscene runs minutes with the mod silent (Escape skips it), and the VM's load is slow.
- Movement skills MAPPED, not built (2026-09-14, `docs/re_movement_skills.md`, static RE only, deferred by decision):
  **no movement skill checks line of sight; the navmesh is the only gate.** Enemy-targeted (Shadow Strike = an INVISIBLE
  CHARGE run, `Skill_AttackWeaponBlink` derives from `Skill_AttackWeaponCharge`; Blitz; the strike/charge runes) need a
  full `Player::CanMoveTo` path or the press is dropped silently (no state, no cooldown, no message). Point-targeted
  (Vire's Might + 44 Forgotten Gods medal runes: teleport / leap / rush / disengage, `targetingMode = Point`,
  `waveDistance` 12-18 u) get their landing from the exported `Character::GetMoveToPoint` (clamp to range along the
  line -> `FindPath` -> `FindStraightMovePoint` re-path -> else your OWN position: fires in place, never refuses). Rush
  is an ordinary pathfound move (goes round obstacles). All gates are exports callable before the press
  (`ValidateEnemy`, `GetMoveToPoint`, `CanMoveTo`). **The four point classes report `GetTargetType()` = 4**, unknown to
  `world::skill_aim`. The mod has no way to aim at open ground yet; the exits lock is the nearest existing point.
- **Both expansions installed + rooms db regenerated for the Forgotten Gods map** (2026-09-14, `docs/rooms.md`
  "Expansion maps"): each DLC ships a complete replacement `world001.map` (base 633 chunks, gdx1 876, gdx2 1582; the
  game mounts the highest, verified live), base chunks are recompiled with DLC props/side areas (164 walkable grids
  differ, mostly < 1 %), three dungeons move by (+224, +160), Gloomwald replaces the cut Prospect Hill corner off
  Burrwitch Village's west edge, Malmouth hangs north of Ugdenbog, Forgotten Gods is an island reached by the
  Emissary's portal. Tools read the overlay through `tools/gdmap/gamefiles.py` (map, arz `Layered`, Text arcs;
  `GRIMDARK_GAME_LAYERS=base` forces the base world); the level-body cache is per map (a same-size rewrite fooled the
  shared one). **Two dbs**: `assets/rooms.db` = gdx2 world (meta.map), `assets/rooms_base.db` = the frozen base one;
  `rooms.cpp` picks by `gdx2/resources/Levels.arc` under the install root. Regen = `rooms.py shift` (moved dungeons)
  -> `rebuild --write --prune` -> `rehome --write` -> `areas --write` -> `seams --write`. Riftgate zones now carry
  acts 5-7 (`gen_riftgate_zones.py` letters h/i/j), 57 shrines. 0I023's nav tiles sit 480 u off its footprint = a stale bake (no live navmesh); `build_area` skips such chunks.
- `/settle` + the all-regions shots tour (2026-09-15, docs/rooms.md "Shots pacing"): `ResourceLoader::IsIdle` +
  `Region::IsLoadingFinished` replace the flat 1.6 s per-sample sleep (3.06 -> 1.27 s a room, shots equivalent);
  `shots.py all --status unseen` tours every region nearest-first with crash relaunch. Built + measured live.
- Masteries reference + nine masteries (2026-09-15, verified live on the dev char): `docs/masteries.md` = every skill of all
  nine masteries with the game's tooltip at level 0 and at max, generated by `tools/gen_masteries_doc.py` from the dev
  route `/masteries` (`gameapi::dump_masteries`, order from the class tables via `tools/arz.py`). `GenerateUISkillText`'s
  `int` is the RECLAIM COST (the exe passes `GetCurrentSkillReclamationCost`), not a level delta -- the text is always for
  the current level, so `gameapi::skill_tooltip_at(skill, level)` raises with `IncrementSkillLevel(n)` and lowers with
  `DecrementSkillLevel(n)`, restoring a learned skill lowered to 0 through `SetSkillLevel(cur)` (the >0 path re-registers
  with the owner, vt+0xc0, which Increment does not); state verified identical after a full dump. `mastery_choices()` now
  lists nine (07-09 = Inquisitor/Necromancer from Ashes of Malmouth, Oathkeeper from Forgotten Gods; the base game ships
  only their placeholder mastery record and "?" tags, so a "?" name is skipped). Dev: `/ui?chars=1` lists the picker's
  characters with the selected one starred, `/ui?char=N` selects -- **check it before Start: the main menu preselects
  the user's last-played (real) character**. The skill list holds every mastery's tree whether chosen or not.
- **C = player characters** (2026-09-16, verified through the loop): `ScanGroup::Players` = is-a `Player` in the sphere
  query (the main player is returned by it), C / Shift+C / Alt+C like the other groups; your own character lands as
  "<game label>, you, i of n" (`push_scan_self`, no distance / bearing) and the lock parks the cursor on you -- the
  way to put a cursor-placed skill under your own feet. Type 2 ("at a target") never needs an enemy except the charges:
  `Skill::GetValidMeleeTarget` / `GetValidRangedTarget` with cursor id 0 search `GetTargetsInRadius` around the
  cursor's ground point and else fire at the point; the 16 `Skill_TargetedSpawnPet` skills (Inquisitor Seal, totems,
  traps, summons) are DBR `targetingMode = Point` but runtime type 2 (docs/skills-targeting.md; the spoken word for them
  is still "at a target" -- wording open). No class skill reads type 3; Nullification and Vire's Might read 4.
- Sonar crowd compression TRIED AND REVERTED (2026-09-20, tester feedback after a day of play; commits e59da54..03c9462,
  removed whole): per-kind range compression of the pulse levels toward the far edge once a kind's mean power
  (sum gain^2 / period, the thing's share of the coming window) passed a cap -- tuned live to cap 5 / pivot 0.5, a
  nine-scarab pack lost up to 7 dB on the near ones. The user first heard it as a subtle improvement; testers wanted it
  gone, the stagger rework (below) is what fixed the pile-up. Lessons kept: the pulse-train math (mean power = sum of
  gain^2 / period, incoherent because of the stagger), that a cap set from "five at 2 u" never fires on real packs
  (they sit at 3-9 u where the log period has thinned them), and the game's key enum: F11 0x55, F12 0x56
  (`tools/exports/keynames.txt`, not DIK).
- Grace periods (2026-09-20, built + core-tested, lock grace NOT yet verified live): (1) the sonar field keeps an id's
  phase grid for `FieldParams::grace_s` 1.5 s after it drops out of the item list (radius-edge flicker, a visibility
  blink) -- an overdue one fires once on return and continues on its old grid instead of being reseeded. (2) The review
  lock survives `kLockGraceMs` 5 s with the target not found by `find_entity` (a 40 u sphere query around the player:
  a kited enemy that fell out of it was unlocked at once and the next enemy key restarted from the nearest -- the
  user's kiting report); the cursor override is off while lost, the keys say "too far away", the lock resumes when
  the id is found again, a death still unlocks at once. `/lock` prints found / NOT FOUND for N ms.
- Sonar stagger reworked (2026-09-20, HEARD: "a subtle but noticeable improvement", kept; the compression may need a
  retune -- since REVERTED on tester feedback; the user's report: periods still aligned): the only
  anti-alignment was the pan-based phase seed, which put a whole flank (five scarabs at pan +1.00) on the same fraction of
  near-equal periods, and nothing ever separated things drifting through each other (0.21 s vs 0.31 s coincide every
  ~0.65 s). Now (1) a new id is seeded at hash(id) of a period (`FieldParams::hash_phase`; the pan seed stays as the
  false setting), and (2) a due time within `collide_s` 40 ms of another SAME-KIND due is pushed later past it
  (`SonarField::push_clear`, bounded), on seeding and on every reschedule. Tests: a drifting pair stays >= 39 ms apart
  at < 10 % pulse cost; eight co-distant things spread over their 0.21 s period. Consequence: in a field denser than
  1/window the push STRETCHES the crowded things' cadences (the window is a same-kind rate ceiling of 25 pulses/s),
  which is the density limit the ear needs anyway. Different kinds never push each other. Knobs `/sonar?window=&hash=&grace=`;
  status prints `stagger: seed by id hash collision window 0.040s grace 1.50s`. The user has an alternative idea in
  reserve; longer play may bring further changes.
- Spirit guide for a one-class character FIXED (2026-09-20, verified live by the user; the report: the guide
  opened a plain skills window): `exe_ui::skills_reclaim_mode()` read the reclaim flag off the pane of the tab the
  GAME shows, and a one-class character's window rests on tab 1 = the class-selection pane (verified live: window
  +0x2630 = 1 with the window closed), which has no flag; the +0x1f4c proxy read 0. RE of the opener (exe+0x21a6f0):
  window +0x2639 = 1 is the exe's own reclaim byte (read back by the window, cleared on teardown), +0x2634 = the npc
  id, vt+0xa8(1) on both pane slots (UISkillPane -> +0x1e4c; class-select pane -> ret). Now: +0x2639, else either
  pane's +0x1e4c, else the proxy; `skills_press_skill` searches both panes for the skill; Undo Points takes the
  screen's tab. Lesson: our screen's tab and the game's window tab are independent -- never key a read on the game's.
- Next (needs the user's hands): player-facing targeting keys
  (nearest enemy / cycle / announce name, distance, direction -- the hover name arrives as `box_font` HUD text),
  an attack key that clicks the locked target, wall-tone tuning by ear, hover sounds, the main menu icon buttons.
- Room titles cleaned up (2026-09-20, `docs/rooms.md` "Duplicate titles"): the tester's "unsuffixed duplicates" were
  cross-sub-region repeats (allowed by the rule, disambiguated by the sub-region word) next to stale " N" suffixes from
  describing before the sub-region pass, plus 61 same-sub-region duplicates from the parallel describer's unlocked
  dedupe. `author.py retitle --write` on both dbs (base 1403 changes, DLC 1566); `save_description` locked. Decided
  with the user: mechanical fixes over re-tagging (a retag loses every title). Open: the exits list still labels by
  title alone. VACUUM would not shrink the dbs (no free pages); zlib on the grid blobs would (~0.3).
- Rooms data moved out of the binary dbs (2026-09-20, `docs/rooms.md` "Data layout"): the committed 41 / 70 MB SQLite
  files could not be diffed and were heading for GitHub's 100 MB cap. Now `data/rooms/<world>/` holds JSONL per region
  (region, sub-regions, rooms, exits, shots; sorted) + zlib'd grid blobs (17 MB for gdx2 instead of 56), and
  `tools/rooms_pack.py` (stdlib-only) packs / unpacks / verifies; CMake builds the two dbs into `build/ninja/assets`
  before the DLL, `package.py` takes them from there, the authoring tools use unpacked working copies in
  `build/rooms/`. Round trip verified identical for both worlds. `gdmap.roomsdb` imports numpy lazily so the packer
  runs on CI's plain Python.
- Telegraph cues re-keyed on exact skill class names, "area" + "charge" added (2026-09-20, `docs/telegraphs.md`
  "Class table"; built, NOT yet heard): a tester's Ancient Shambler report led to the avalanche being cued as a stomp;
  the survey showed substring matching mis-cued ~10 % of active monster skills (auras, rains, drops, lightning bolts,
  on-hit novas, teleports). Unknown classes are silent and counted. Open: dying skills (the Shambler's death burst).
- **Rune of Hagarrad is a proximity mine, not a turret** (2026-09-20, static RE, `docs/re_pets_gamedll.md` s.8): the
  pet record's `deathFromEnemyRange` 2.0 / `deathFromEnemyDelay` 1.0 s drive `ControllerMonster::Update` -> after the
  arm delay, every 200 ms `DieIfEnemyInRange` sphere-queries the range and `ControllerCombat::KillMe`s the rune on the
  first live hostile `Monster`; the icicle ring is the `dyingSkillName`. "Dies fast" is the design: one enemy stepping
  within 2 units spends the rune. 30 s is the timeout, 5 the summon cap, 4 s the cast cooldown.
- **The free cursor** (2026-09-20, built, NOT yet run in the game -- the user's session held the DLL at link time):
  "point we click" divorced from "thing in the scanner". Shift+W/A/S/D move the virtual cursor's world point
  (`world::free_cursor_step`, `src/core/cursor_step.{h,cpp}` for the arithmetic, doctest'd), Z toggles grid / polar
  ("cursor mode default" / "cursor mode polar", persisted `cursor.polar`); everything else is silent. State: a third
  cursor source beside the entity lock and the point lock -- `g_free_cursor`/`g_free_point` override the projection in
  `world::tick`, `virtual_cursor_pos`, `aim_along_line` and `press_point`, while the lock is left as it was, so the
  reviewed thing stays reviewed and the next landing resumes it. Seeded on the first press from the current cursor.
  Game side: `hooks::set_game_key_filter` now gets the release flag and the EVENT's own shift / ctrl flags
  (`key_source()` is updated after the filter, so it still showed the previous event's modifiers: a plain W right
  after Shift came up would have been swallowed); app.cpp swallows the Shift+WASD PRESS only (a swallowed release
  would stick the exe's held byte: the same trap as the off-window mouse transition). README
  "Advanced targeting" + docs/controls.md rows. Dev: `/freecursor`. To verify live: seed from a locked enemy, step,
  `/freecursor` screen point vs `/project?pts=`, J at the point, then `.` re-locks.
- **Vanished targets keep the cursor; the polar line keeps its heading** (2026-09-20, same session, not yet run): the
  entity lock's grace expiry and the dead-Monster release now `lock_point` the last position the entity was found at
  (`g_lock_last_pos`, recorded per found frame) instead of `unlock_target`, and the grace period projects that point
  instead of dropping the override -- the user's call: a target that dies or runs off must not snap everything back to
  "no target". `step_cursor` takes a heading the caller keeps (`g_free_heading`): Back through the player then Forward
  goes out the same line (the cursor is always the far end), a turn on the player turns the line.
- **Crash 2026-09-20 18:51, a freed entity's position** (log: `exception 0xc0000005 at Engine.dll+0x22bdf7 (read 0x3D)`,
  rax = 1 = the WorldVec3's Region*, inside our Engine::Update tick one frame after the mod released a J hold on an
  enemy that had just died): `WorldVec3::GetWorldPosition` dereferences the region first thing; the WorldVec3 came from
  `entity_world_vec` on a dead object -- `Entity::GetCoords` on freed memory returns garbage without faulting and the
  region check was null-only. The pointer most likely came from `gameapi::object_by_id`, an id -> pointer cache
  refreshed every 600 frames (the "never hold entity pointers across frames" trap, again; `entity_position` from
  combat.cpp's debuff-on-me panning or casts.cpp), or from `reping_tick`'s per-frame re-find of a reviewed id that
  stays set after the monster dies. Fixed three ways: `object_by_id` checks every hit against the exported
  `ObjectManager::IsObjectIdOnDeletedList` (newly wired) and drops it; `entity_world_vec` refuses a Region* that is
  not a real address with a vtable in the game's images (`plausible_region`); `world_pos_of` runs GetWorldPosition
  under SEH and returns zero on a fault (logged). Not yet exercised live.

## 2026-09-21 -- Alt+Enter: a vanilla in-world crash, now blocked
- A player reported Alt+Enter leaving the game deaf to the keyboard with NVDA locked up. Reproduced with inject.ps1: it
  is a plain game crash (`exception 0xc0000005 at Grim Dawn.exe+0xa2efc`, read of `pages[-1]`, rcx = an encoded heap
  block header), and a crashed Grim Dawn looks alive from the outside -- the main thread sits in Crate's CrashReport.dll
  waiting on `crashreporter.exe` while the sound threads (the game's and our miniaudio loops) keep playing. NVDA stalls
  on the window whose message loop is dead; no keyboard hook is involved. `gd.py status` says CRASHED.
- The path (static, exe dump + Engine.dll): `WinWindow::WindowProc` maps WM_SYSKEYDOWN VK_RETURN + Alt (Engine.dll+0x217e1a)
  to the exported `WinWindow::OnToggleFullscreen` -> the exe's handler changes the mode -> the main loop's resize block
  (exe+0xedf0) calls exe+0x20890 with "show options" = true, which reads the OLD Options screen's tab index
  (`[InGameUI+0x4def8]->+0x90->+0x280`, -1 when the pause menu's Options was never opened: the host ctor exe+0x29efc0
  leaves +0x90 null), deletes and reconstructs the InGameUI (0xac998 bytes, ctor exe+0x205a70; the `loc:` burst of every
  option tag in the log is the fresh Options screen's ctor exe+0xc8e60, which ends with SetCurrentTab(0)), re-shows the
  host (exe+0x21c1d0 -> host show -> OnShow exe+0x29f2d0 constructs the screen) and calls SetCurrentTab(-1)
  (exe+0xcd300): `cmp [+0x280], -1` differs from 0, so it takes `pages[-1]` from the vector at +0x288 as a page and
  AddChild (exe+0xa2ee0) dereferences it. The main menu's resize path (exe+0xbe780) is different and survives.
- Fix: `WinWindow::OnToggleFullscreen` is hooked by export and refused with one spoken line
  (`strings::kFullscreenToggleBlocked`). Window mode remains the Options, Video setting (which rebuilds from an open
  Options screen, a valid index). README + docs/controls.md carry the key.


## 2026-09-21 -- installer: Launch button removed
- Defender flags the mod (an injector plus a hooking DLL, as expected) and a folder exclusion does not let a process
  spawned BY the installer start gdlaunch.exe, while the desktop shortcut works. Rather than chase which of
  SmartScreen / the exclusion scope / the parent process is responsible, the button is gone: the installer's closing
  dialog already points at the shortcut. `installer/src/gui.rs` only; `paths::launcher_path` stays (the
  installed-check uses it).

## 2026-09-22 -- wall tones: the walk probe replaces the rectangle
- The user's report: a wall tone north at the foot of the Four Hills rope bridge, which is walkable. Cause: the bridge
  runs ~28 degrees east of the tones' north axis and is ~2 u wide, so every lane of the straight rectangle hit its
  west rail within 2-5 u. The tones now simulate holding each key instead (`world::walk_sim`, the game's own WASD
  request + snap + fallback; docs/re_wall_sliding.md section 8) and read the distance along the key before the walk
  stops or leaves a truncated cone (the user's shape: 1.5 u at the feet, 20 degrees per side). Simulated paths match
  the real character to hundredths of a unit at the bridge and at the Burrwitch rock pile. ~0.7 ms per frame.
- F12 toggled walk / rectangle for an A/B by ear; the user kept the walk and the rectangle and F12 were removed the
  same day. Knobs `/walltones?h0=&deg=`; dev route `/walksim` (modes, trace, per-call timers). The dev server's f11/f12 key names were DIK codes (0x57/0x58), now the
  game's 0x55/0x56.
- A hot reload in the world crashed once in `ScreenManager::resolve` (a screen's is_active) on the first frame after
  re-inject; no fault record; the next reloads were fine. Unexplained.
- Reading a route for the player: `/findpath?x=&z=&y=&corridor=1` gives the navmesh corridor even when `Player::FindPath`
  refuses it (result 2 = the detour-ratio gate, Player+0x4bf0 = 2.5: a route longer than 2.5x the straight line; the
  Rotting Croplands Detonation Site on its ledge was 193 u for 34 u straight). Neither WASD nor a click will take such a
  route; the corridor sampled against the rooms grid names the rooms it crosses.

## 2026-09-22 -- sonar: shrines disabled at this difficulty no longer ping
- The user, Hallowed Hill on Veteran (= Normal): a "ruined shrine" ping 0.8 u away that could not be used or targeted.
  It is the Forgotten Gods devotion shrine `devotionshrinen04` (`normalDisabled = True`; Elite / Ultimate only, Aether
  Shard offering). `StaticShrine::InitialUpdate` (Game+0x551b20) skips SetShrineState when +0x555 is set, so the state
  (+0x578) stays 0 and `IsOfInterest` (Game+0x552f80: state 1 or 4, or 6 with +0x555 clear) is false. A screenshot
  confirmed the game does not draw it at all. The sonar's Shrines group now requires IsOfInterest, like N already did;
  on the difficulty that enables the shrine it passes again. (`/entities?src=frustum` returned an empty set even
  unpaused; not investigated.)

## 2026-09-22 -- non-English text: map names and type-ahead keep non-ASCII letters
- A player on a non-English game language: the Ctrl+M map list mangled names with diacritics. The map's own icon names
  (`world::map_markers`, read from the nuggets as UTF-16) went through an ASCII-only narrowing that replaced every
  non-ASCII character with '?'; they now use `log::utf8` (WideCharToMultiByte) like every other game string.
- The same player would hit the list type-ahead: `GraphNavigator` turned each typed non-ASCII character into '?' before
  the search, which already folds Latin diacritics and case. It now appends whole code points
  (`TypeAheadSearch::add_codepoint`, surrogate pairs combined), backspace removes a whole UTF-8 sequence, and the
  repeat-letter cycle compares code points. Tests: accented and Cyrillic letters (tests/typeahead_tests.cpp). Not
  verified with a real non-English keyboard layout: the typed text is the game's own per-key character.

## 2026-09-23 -- the route ping rebuilt, and direct aim (F12)
- The review ping's three cues came from wotr-access, but the port kept only the names: WotR probed sight first, then a
  real path search. Grimdark sampled the straight line every half unit with `IsPointOnPathMesh` (a bounding-box test)
  and called the target "unreachable" when its own spot failed that test. No path search, no sight test. So players
  walked to "unreachable" things and hit "path around" ones.
- Measured with the new dev route `/hitlos[?max=&ring=1]` (monsters plus 120 ground samples), in the FG hub and
  Burrwitch Outskirts: "straight" was always hittable; "path" was hittable 11/15 and 26/31 times; "unreachable" about
  half the time. `Skill::IsTargetInLOS` (Game.dll 0x4830e0 by id, 0x483380 by point; `this` is never read) is the
  game's spell/projectile sight test: a ray from a named point on the caster through `World::CheckLOS`. It is not the
  camera. Clicking is the camera pick ray (`/los?id=`); outdoors that is rarely blocked, but 5 of 13 hub guards were
  simply off the window.
- `world::route_kind` checks in this order:
  - unreachable: `NavManager::FindPath` fails, or its endpoint is more than 1.5 u (plus the target's footprint) away
    or 1.5 u off in height. The same arrival gate as the rooms exits.
  - straight: `IsTargetInLOS` is clear and the path is at most 1.2x the straight distance + 1 u.
  - path: anything else.
  It costs 36-85 us per check (still every frame in `reping_tick`). Not tuned by ear yet.
- A fourth cue, "can't click" (off the window or the camera ray blocked, `review_los.wav`), was built and dropped the
  same day, never released: direct aim makes clickability irrelevant.
- Direct aim (`world::direct_aim_apply`, always on; an F12 A/B toggle and `/directaim` existed while the user played with
  it and were removed the same day, as was the old half-unit ping probe). Static RE: every
  skill request reads the controller's combat enemy +0x468 (`SetCombatEnemy`; for target types 2/4
  `DefaultRequestSkillAction` ignores its id argument) and a hot slot fires at the mouse repeat data (+0x43c id,
  +0x440 WorldVec3, `SetMouseRepeatData`), which the exe writes from the cursor pick on every mouse event. The pick is
  exe code with no single store, and its Engine calls are shared, so it is not a clean seam. `docs/re_movement_skills.md`
  had +0x468 / +0x46c swapped: +0x46c is the combat ALLY. The mod now:
  - hooks `HotSlotOptionSkill::Activate` (Game.dll 0x3077a0, a unique body) and writes both fields for the locked
    target right before a slot fires;
  - makes the `HandleActionFromMouse` hook substitute the locked id and position.
  The game's range, walk, clamp and vox rules all still apply. The real cursor keeps its old placement (on the target
  when shown, else where the line to it meets the window's edge), because the exe drops a mouse event whose pick finds
  nothing.
- Verified live on `claude`, Weapon Attack in quickbar slot 1, training dummies 25-28 u away and off screen:
  - off: key 1 swung in place;
  - on: key 1 walked 23 u and attacked;
  - on: J (pressed at the window's edge, pixel 4,651) walked 28 u and attacked.
  Not verified: I, a held slot key, a real spell, Sky Shard / Seal placement, a locked point (exit), the sticky target
  during a mouse hold. The game's own hover (the HUD target name, highlights) still follows the real cursor.
  The user then played with it (the same day) and chose it as the targeting model.
- Shift is passed to the game in the world (its Hold Position key: the exe's key action sets X+0x848 on down,
  exe+0x27020, and clears it on up, exe+0x26c41; `HandleActionFromMouse` then stops movement and acts in place), and J / I
  no longer refuse to fire while Shift is held (Ctrl / Alt still block them). With the game's Classic Casting option a plain
  press walks into a spell's range and Shift casts in place. Not verified live.

## 2026-09-23 -- pitch options, walk-to (L), line-of-sight preference
- `audio::play_sample` gained `semitones` (a resampled copy, cached per file and shift) and `predelay_ms`.
- Ctrl+T, two new rows with Space tooltips (`src/cues.h`):
  - range pitch (off by default): the sonar's enemy ping is +4 semitones within 20 u and +8 within 3 u. 3 u is
    `meleeTargetDistance` 2.4 + the 0.5 skill-use tolerance.
  - height echo (on by default): `;` and `'` play the route sound a second time 90 ms later, +4 semitones for a
    target more than 2 u north (-z), -4 for one more than 2 u south.
- T: "prefer enemies in line of sight" (off by default). The enemy review cycle sorts by distance + 10 u when
  `Skill::IsTargetInLOS` fails.
- L = walk to the reviewed thing: `ControllerAI::MoveTo(point, 0, 0, 5, Character::GetRunSpeed(false))`, the call the
  game's MoveTo state makes (Game.dll 0x152e16), issued directly so `DefaultRequestMoveAction`'s WASD direction gate
  (docs/re_wall_sliding.md) never applies. An exit walks to the neighbouring room's most open point
  (`LabelGrid::open_point`, unit-tested). U stays the game's Interact.
- Verified live in the FG hub: the walk reached an off-screen training dummy 23 u away (no attack) and a guard whose
  straight line was blocked for walking and sight (~2 s, routed); an exit walked into the encampment courtyard's
  middle; the new settings rows and their Space tooltips work and persist; the sonar fired with range pitch on.
  Not verified: a cross-region opening, the line-of-sight ordering with mixed visibility, anything by ear.
- Known limitation, skipped by decision: the route ping's `NavManager::FindPath` treats a closed auto-opening door as a
  wall, so Captain Bourbon (Devil's Crossing, behind the Burrwitch Prison Gate, a `FixedItemDoor` with a trigger volume)
  read "unreachable" while L walked straight to him: the crowd movement goes through and the trigger opens the gate.
  With the gate open the query found the route. The user knows of no other such door in acts 1-3; the rest need an
  explicit interaction. Fix if it matters: find the crowd's door filter.
- The route ping's echo was pulled in to 30 ms and down 12 dB after the first listen (it was 90 ms at equal level).
- After the user's first session: range pitch and the line-of-sight preference default ON (not jarring / liked), the
  route ping echo defaults OFF (too much for players who do not want it). The line-of-sight row moved to its own T
  Tab stop, "targeting", the home for the review-cursor options to come. An existing settings.txt keeps its values.
- Combat voice fix (2026-09-23): `EventManager::Send` 0x1b also draws pickups and rewards in their own styles
  (gameengine.dbr: `relicPickupStyle` = a component, the potion / money pickup styles, experience / faction / skill
  point gains). combat.cpp cut the style path to 31 characters, so every event looked alike and a component
  auto-pickup was spoken in Mark. The style now keeps 95 characters: `style_floatingtext_combat*` goes to Mark as before,
  the `*pickup` styles (component, potions, money) are said verbatim in Zira (the user: they belong to the player's
  voice, not silence), and the reward styles stay silent (experience has its own polled announcement). `/combat` marks
  each. Not verified live (needs a pickup).

## 2026-09-23 -- armor on the stats tab; armor is per body region
- Static RE (two subagents): each hit rolls one body region (`CombatManager::PickRegion` from `TakeAttack`, weights from
  combatformulas.dbr: torso 26, legs 20, head 15, shoulders 15, arms 12, feet 12) and only protection tagged with that
  region, or with region 0 (skills, devotions, jewelry, weapons), applies (`CombatAttributeDefense_AbsorptionProtection::
  Execute`, 0x105100). Titan Quest's system, live; the wiki does not mention it.
- The sheet's "Armor Rating" (exe+0x13e28b..0x13ed2b) is the expected armor per hit: flat + sum of chance% * region
  armor, after the armor % modifier (defense modifier 0x28), rounded. Armor = defense type 0x26, absorption 0x27.
  `gameapi::armor_breakdown` reproduces it: per equipped item a `CombatDisplayAccumulator(true)` filled by the item's
  vtable slot 0x4c8 (checked against the exported `*::GetDefenseAttributes` overrides), location -> region (7 head,
  8 chest, 0xa arms, 0xb legs, 0xc feet, 9 shoulders; anything else is flat), non-item armor = the
  `GetAllDefenseAttributes` total minus the items'. The stats tab shows it after DPS, with the rollover's per-region
  rows as the row's columns.
- Open: belts are region 7 (waist), which `PickRegion` never rolls, so belt armor looks unused in combat; the sheet
  counts the belt's location as flat armor. Unverified which is right. Not verified live yet.

## 2026-09-28 -- navmesh reach and finding unexplored ground (dev routes /pathprobe, /fogmap; measured live)
- **Reach**: the live navmesh only exists for the regions streamed in around the player. At Vanguard of the Three
  (regions are 128 u squares), points snapped onto the mesh out to ~220 u straight / ~270 u of path; past ~300 u
  `FindClosestPointOnPathMesh` finds nothing in any direction. Across region borders `NavManager::FindPath` works
  (143 of the reached targets in a 200-u sweep were in other regions). Cost: ~0.15 ms per query (max 0.35 ms), a
  failure ~0.15 ms. Detour's own caps in `NavManager::FindPath` (Engine+0x10c2c0): 1024 path polygons (0x400), 2048
  straight-path points (0x800); the streaming radius binds long before them. Anything farther needs our offline data
  (the rooms grids / `tools/gdmap` navmesh tiles), not the live mesh.
- `FindClosestPointOnPathMesh` returns NavResult 1 = found, 2 = no mesh within the radius (and leaves the out
  WorldVec3 garbage). `/pathprobe?x=&z=[&snap=]` = snap, then `NavManager::FindPath` with length + corridor, timed.
- **Fog of war** (`Region::GetFogOfWar(false)`, Engine `FogOfWar`): a byte per 8x8-u cell, data +0x10, width +0x18,
  height +0x1c = 18x18 per region (16 inner cells + a ring overlapping the neighbours); cell of region-relative (x, z)
  is c = int(x/8 + 1), r = h - int(z/8 + 1) - 1; > 150 = fogged, 0xff = never seen (`FogOfWar::IsInFog`, `Initialize`).
  `/fogmap?n=&range=&grid=1&unsee=` merges every loaded region's inner cells into one world map, takes fogged cells
  nearest first, snaps each (6 u) and paths to it: all 22 regions within 300 u in 2-30 ms.
- Live result at the hub: all ~2200 fogged cells in range were OFF the navmesh (cliffs, drops, rock) -- walking reveals
  every walkable cell within sight, so the fog left near an explored area is over unwalkable ground and "nearest
  unexplored" is often "none within the loaded range". With `unsee=60` / `120` (seen cells beyond that treated as fog,
  analysis copy only) the finder returned the walkable edge at that radius with real path lengths (96-174 u) in
  2.4 / 6.2 ms. Candidates are ordered by straight distance; nearest by PATH needs pathing the top few and taking the
  shortest.
- Trap: `/teleport` into Basalt Crags left the test character dead and then back at the hub; check a spot first.
- **Distant fog** (same day): every region's mask is available two ways, and they agree (5 distant regions compared
  cell-count for cell-count, and 0J057's grid exactly):
  - LIVE: `Region::GetFogOfWar(false)` on ANY region object, loaded or not. An empty region+0x138 is filled lazily
    (Engine 0x194f10) from `World::GetFOWManager()` (a map of region name -> blob, the name being Region+0x48, e.g.
    "Levels/Region0J057.lvl") via `FOWManager::Retrieve` -> `FogOfWar::Read`; a region with no entry is initialised
    all-fogged. `/fogmap?range=1500` read 172 regions this way with the game healthy. Cost: each first read allocates
    a FogOfWar and a small GPU texture (`FogOfWar::Read` calls `CreateTexture`) that stays on the region -- what the
    game's own map does. Reading the FOWManager's blobs directly would avoid the texture if that ever matters.
  - OFFLINE: the save's `map.fow` (Steam cloud: `userdata/<id>/219990/remote/save/main/_<char>/levels_world001.map/
    <Difficulty>/map.fow`), all 1014 regions, as of the last save. `tools/fow.py` decodes it (format in its docstring:
    u32 size + one LZ4 block of "FOWX" v1 entries). The block's last sequence reads 4 bytes past the file end (the
    game's `LZ4_decompress_fast` does not care), so the python `lz4` package refuses it; fow.py carries its own decoder.
- So distant fog is solved; what distant exploration still lacks is distant WALKABILITY and routes, which the live
  navmesh cannot give past ~250 u -- that has to come from the offline rooms / gdmap data.
- **Fog x rooms** (`tools/fog_rooms.py <area>|all`): each room's walkable label cells looked up in its engine chunk's
  fog (chunk placement from world001.map) -> fogged fraction per room. Numpy over a whole area's 0.25-u grid is
  seconds offline; the per-room answer is the natural unit for "where haven't I been" (the test character: vanguard
  0 partly / 0 never; aetherfire 398 seen / 23 partly / 18 never of 439; alpinefort 21 / 44 / 235 of 300). This is the
  distant-walkability piece the live navmesh lacks, and the room graph (exits) gives the route. Caveats: fog is 8-u, so a
  room brushing a fogged cell edge reads a few percent fogged (threshold, not 0); lvl files reused by several map
  records share one fog key (fog.get by lvl path, placement = the first record) -- not yet checked whether that ever
  matters.

## 2026-09-28 -- L walks to the last selection; P finds unexplored ground (verified live)
- L targets the last selection: a review landing, or the follow target (a map pick, `'`, P). It always just walks:
  a route check with "no path" / "too far" and a "stopped" stall report were built and then removed by decision (a
  check misreports the gate the crowd walks through; a stall report is over-convenient -- players hear whether they
  walk).
- P = the nearest fogged 8-u cell within 250 u that snaps onto the navmesh and has a complete path, nearest by walk
  among the first 8 reachable; it becomes the follow target "unexplored area" and "explored" is spoken once its cell
  clears. Live in Mourndale (test character): "unexplored area, 107 away, 11 o'clock" (1 of 41 candidates reachable),
  L walked it in ~12 s, "explored" on arrival; the next P said "nothing unexplored within about 200" -- the never-seen
  rooms fog_rooms.py lists were ~195 u off with no navmesh loaded there. At Vanguard: nothing (fog only over cliffs).
- Bug found and fixed: the route ping built P's target in the player's region and PutOnFloor'd it to -0.8, under a
  walkable platform at 5.1, and read it "blocked". `find_path_corridor(..., floor)` / `route_kind(..., floor)`: P's
  exact navmesh point is no longer floored. Other pings unchanged.
- Traps: a hot reload in the world left the game paused again (L "fired" and nothing moved; `/pause?set=0`). `/fog` is
  the REVEAL route (FogOfWar::AddVisibility) -- reading fog with it clears it; use `/fogmap` or tools/fow.py.

## 2026-10-02 -- target type 4 skills reach the hotbar manager (verified live)
- Report: Vire's Might missing from the Ctrl+` picker. Cause: its runtime `GetTargetType()` is 4 (all four point
  movement classes plus Nullification's `Skill_DispelMagic`, `re_movement_skills.md` s.1), `world::skill_aim` mapped it
  to None, and the picker drops None as passive. Now 4 -> `AtPoint` ("at a spot"). `/masteries` shows both as
  `at a spot|4`; docs/masteries.md regenerated (only those two rows changed).
- Firing goes through direct aim unchanged: a type-4 request reads the combat enemy +0x468 (the locked entity) and the
  hot slot fires at the mouse repeat WorldVec3 (the locked point / free cursor). Verified: Vire's Might on slot 1,
  `/lock` on a Seeker 13.0 u away, `/hotbar?activate=0` -> 2.1 u away (charged). The point-lock / free-cursor case was
  not tried. The design question in `re_movement_skills.md` s.8 (a
  direction-and-distance aim for runes) stays open.
- The "claude" test char was level 24 with ONE mastery allowed: the big `/cheat?xp=` jump skipped the game's per-level
  `SkillManager::UpdateMasteriesAllowed(level)` step (Game.dll 0x51f740: `allowed += 1` if `level >= threshold[allowed]`,
  one step per call). New `/cheat?masteries=1` calls it with the character's level until caught up. The char is now
  Soldier + Oathkeeper (Oathkeeper 10, Vire's Might 1), via `/ingame?action=2`, `/skills?pane=8&tab=1`, `/skills?learn=`.
- Our own Vire's Might logged `telegraph: unknown skill class 'Skill_AttackPathCharge'`: `on_cast` looked the class up
  before skipping friends. Now the `world::is_foe` test runs first: players (ours or, one day, another player's), pets
  (class `Monster`, so a `Player` class test would miss them) and allied NPCs never reach the table, and the unknown
  list only ever holds enemy vocabulary. Pets not tried live (the char has none now). PathCharge stays out of the table: no monster record uses
  it. Verified: a second charge counted `friend=1 unknown=0` in `/telegraph`.

## 2026-10-02 -- Ctrl+' follows the reviewed thing (verified live through /action)
- Ctrl+' (`follow.review`, `world::follow_reviewed`) makes the review target the follow target by id (entities
  re-resolved on every use, a review point stays put), flagged `g_follow_review`; `set_follow_target` (a Ctrl+M pick, P)
  clears the flag. ' in that mode (`follow_review_land`) re-locks the review cursor on it, restores `g_reviewed_id` /
  the label and plays `ping_reviewed`, silent like ;; "<label> gone" when `find_entity` fails. `land_on` now keeps the
  plain label in `g_reviewed_label` for this.
- Verified: enemyNearest (Training Dummy) -> follow.review "following Training Dummy" -> neutralNearest (lock 57393, the
  rift) -> follow.ping -> lock 60725 (the dummy), nothing spoken. Not tried: "gone", a moving target, the real Ctrl chord.

## 2026-10-02 -- devotion "game counts 19 spent" with 13 stars learned: not a discrepancy
- The user's real character dumped "13 of 55, game counts 19 spent". `SkillManager::GetNumDevotionPointsSpent`
  (Game.dll 0x51fc90) sums `Skill::GetCurrentLevel` (vt+0x1c8, our `g_s_curlvl` slot 57) over every devotion skill
  (`+0x4a0` set), and a celestial power star's current level is its power level: Turtle Shell at 7 counts 7, so
  13 - 1 + 7 = 19. Points are consistent (13 earned, 13 stars, 0 available). Only the dev dump read it; it now prints
  the stars learned and labels the game's figure.
- Same session, read-only on that character at a spirit guide: our reclaim gate refused Vulture (sole Chaos source,
  needs Chaos 1) as self-locked and Tortoise's power star as "would lock Scarab", as the rule predicts. The game's own
  flags (`Skill+0x568` self-locked, `+0x550` dependency names) were all zero: the exe only fills them while its devotion
  map is drawn in reclaim mode, which the mod never shows, so they cannot be compared without calling
  ComputeReclaimBlockers ourselves.

## 2026-10-02 -- clear all constellations at a spirit guide (verified live)
- Why: a self-sustaining constellation set cannot be unwound star by star without a spare point, and the Tonic of
  Clarity is two expansions beyond any blind player. Checked first that vanilla has no apply / batch path: the exe calls
  `UseDevotionReclamationPoints` twice, a star click (n = 1) and Undo (n = the window's negative session count, the
  refund branch that also winds the price counter back).
- Test char setup: `/cheat?aether=30`, `/devotion?take=` Crossroads Chaos + Vulture, `/devotion?reclaim=41590` (the
  crossroads) -> Vulture "Devotion Point Cannot be Removed". `/reclaim`, Constellations tab: row "clear all
  constellations, 6 devotion points, 150 iron bits and 6 aether crystals"; yes -> "all constellations cleared", 7 points
  available, no affinity, 3875 -> 3725 bits, 30 -> 24 aether. Not tried: a bound celestial power, a price crossing a tier
  step (the replica follows the asm at Game.dll 0x520050 and is checked against the first point).
- The test char's Oathkeeper / Vire's Might from earlier today was gone after the user's session: never saved
  (learned through dev routes, the game was closed without an autosave in between).

## 2026-10-02 -- embedded version + update check (unit-tested; not run in the game)
- Version: CI's new "Version" step exports `GRIMDARK_VERSION` (the tag, else `ci-<short sha>`) before the build; CMake
  configures it into `build/generated/grimdark_version.h` (`gd::kGrimdarkVersion`) and `build/version.txt`;
  `package.py` now ships that file and refuses a `--version` that disagrees. Local builds are `dev`.
- Update check: after "Grimdark loaded" a worker does ONE WinHTTP GET to api.github.com (release builds
  `/releases/latest` -> `tag_name`, semver compare; CI builds `/releases/tags/ci-latest` -> `target_commitish`, newer =
  a different commit since CI never moves ci-latest backwards) and speaks, not interrupting, "Grimdark update available
  v0.5.0, you have v0.4.0, run the Grimdark installer to update" (CI: "newer Grimdark CI build available <sha>").
  Silent on dev / offline / non-200 (logged). `grimdark_unload` closes the in-flight request and joins. F1 -> mod options
  "check for updates" (settings `updatecheck`, default on). WinHTTP over the alternatives: built in, Schannel TLS, system
  proxy, no new dependency; JSON is two top-level string fields, read by `core::update::json_string`.
- Verified: gdcore_tests (4 new cases); a full build into build/vertest with GRIMDARK_VERSION=v0.0.1 embeds the string;
  both endpoints answer (latest v0.4.0, ci-latest 26ab370). NOT run inside the game: the user held it.
- Follow-up (user test of v0.5.0 -> v0.5.1, it worked): the line was spoken right after load and stomped by the main
  menu's own announcements. The worker now only stores it; `update::tick` (from `app::tick`, game thread) speaks it once a
  screen other than "loading" has been current for 3 s. Live (build configured as v0.5.0): that spoke 2.4 s BEFORE "Main
  menu" -- the title / intro sit on the "unsupported" fallback first. Now gated on `main_menu` / `in_game` only: "Main
  menu" 17:02:18.649, the update line 17:02:21.674. Offline path via the new dev knob `GRIMDARK_UPDATE_HOST=nonexistent.invalid`:
  `update: request failed (12007)`, nothing spoken, main menu normal. Build reconfigured back to dev afterwards.

## 2026-10-05 -- two-handers lost on equip (tester report, reproduced and fixed live)
- Report (0.5.2): Enter on a two-handed sword in the bag put the club + shield in the bag while the equipment still listed
  them, and the sword was gone. Reproduced on a fresh character ("Testbot", Scrapmetal Greatsword spawned with the new
  dev route `/inv?spawn=<record>`; `/cheat?xp=` + `/skills?attr=1` for its Physique 53).
- Cause: `EquipmentCtrl::SmartAutoInsert` places the item itself only when it displaces at most one; for a two-hander
  over weapon + off-hand it lists both and returns true WITHOUT placing (Game.dll+0x277090: `displaced.size() >= 2`
  skips `PlaceItem_HandRight`). `use_item` took true as "equipped": the sword left the bag for nowhere (the object lives
  on until the game exits -- `/inv?give=<id>` recovered it in-session) and the pair was added to the bag while still held.
  The exe's own path (exe+0x1eb4c6..0x1eb985, read this session): bag RemoveItem(new) FIRST, then
  `IsSpaceAvailable(a, b)` (the two-item overload); both fit -> AddItem both, `EquipmentCtrl::RemoveItem(off-hand)`,
  `PlaceItem(RightHand, new)` (which detaches the weapon); one fits -> the other onto the cursor; none -> error sound and
  the new item back into the bag. Ours: `swap_in_two_hander`, refusing (`kBagFull`) where the exe would use the cursor.
- Second bug, found while testing: the equip picker's `equip(id, RightHand)` with a two-hander displaced only the right
  hand and left the shield in the left hand beside it; putting gear back from that state lost the two-hander again.
  Two-handed = is-a `Weapon` and (`Weapon::IsTwoHanded` -- weapon type vt+0x6b0 in 8..9, the ranged ones despite the
  name -- or `Weapon::IsTwoHandedMeleeWeapon`, 15..18); such an equip into a hand slot takes the same swap. The
  single-slot equip now also checks room for the displaced item (after removing the new one, as the exe does).
- Verified live, every order from a clean state: bag Enter; club-first / shield-first back; the two-hander via the Right
  Hand slot over club + shield; over the club alone; full bag (refused via the real screen, spoken "no room in your bags",
  nothing moved); after dropping two items, the real Enter equips. Equipment tab reads the two-hander in both hands.
- Dev routes added: `/inv?spawn=<record>` (a new item from a database record, via `Item::CreateItem`), `/inv?detach=<id>`, `/inv?give=<id>`.

## 2026-10-05: dev keys take the player's path
- Synthetic `/key` events used to be appended to the game's key queue unfiltered (and recorded for the mod only when
  the game fetched them), so every dev key reached the game AND the mod. In the world that made a dev P the game's
  pause as well as the mod's "find unexplored", and the following L walk silently did nothing -- the dev loop could
  not reproduce a player's session. Now `GetNumKeyEvents_hook` records the frame's synthetic group for the mod once
  per frame and serves the game only the events the game-key filter passes, exactly as for real keys.
- Verified live in the world: P no longer pauses; N then L walks (Personal Rift, 4 away); a held W still moves the
  character; `/key?name=1&ctrl=1` reaches the mod's quickbar read. `/action`, `/jkey` and `/keydown` remain.
- Same day, a regression caught before release: the mod's own injected keys (the lifted Ctrl+M -> plain M for the game,
  the conversation's Escape) share the synthetic queue, and filtering them made Ctrl+M the mod's "next loot" and never
  the game's map. `SynthKey::player` now marks dev keys (`/key`, `/keys`, `/keydown`): only those are recorded and
  filtered; injected keys go to the game alone. Verified live: Ctrl+M opens the map; P, N + L, W as above.

## 2026-10-05: L and off-navmesh targets (Old Arkovia mine entrance)
- Report: from Old Arkovia's riftgate, picking the Mine Entrance on the map and pressing L did nothing. Measured live on the
  user's character: the target was right (the map marker `poi_genericmineentrance` at (-902.1, -16.3), the door at
  (-902.7, -17.2)) and `NavManager::FindPath` found a route, but `ControllerAI::MoveTo` silently refuses a destination
  that is off the navmesh -- by 0.1 u for the marker, 1 u for the door (inside the doorway); the mesh edge is z -16.2.
  Every point on the mesh along the same line walked, across region boundaries too (the region-relative WorldVec3
  hypothesis was tested and is not it). `walk_to` now snaps the floored target with `FindClosestPointOnPathMesh`
  (8 u, as /pathprobe) before MoveTo. Verified: the marker and the door points both walk to the edge.
- Devil's Crossing (Carlile, Sahdina from the rift or the dummies) is the other known limit: the Burrwitch Prison Gate
  is closed until you are next to it (trigger volume), the pathfinder treats it as a wall, and MoveTo walks to the
  nearest reachable point of its partial route instead. Not fixed.
- Dev keys now take the player's path (`SynthKey::player`, see above); our follow statics can be read live by
  resolving them in the PDB (dbghelp `SymEnumSymbols` on the injected grimdark.dll) and `/peek`.

## 2026-10-08: game patch 1.3.0.8 -> 1.3.1.1 (Hotfix 1), first relocation
- New build: exe 0x6ac7f680, Engine 0x6ac7f5f7, Game 0x6ac7f64b (archived as `1.3.1.1-6ac7f680`). Exports: nothing we
  hook changed (gd_names.h regenerated byte-identical); additions are item ascension, combo charges, shadow distance,
  `AddTimeToLive(unsigned, int)`.
- `tools/exe_reloc.py` written for it (the plan of 2026-08-25). Normalized-body matching over .pdata (RIP/branch
  targets masked, IAT calls by import name) gave 6215 unique identical functions; first-come propagation then picked
  wrong twins among the many identical small functions (HandleKeyAction "moved" 0x211980 -> 0x21a880), so propagation
  is a vote fixpoint (identical pairs weight 10, changed pairs 1, vtable slots 10, identical body preferred) plus a
  local similarity fallback at the neighbours' delta: 7995 matched. Leaves without unwind data (thunks) map by a
  24-byte match, disambiguated by the neighbours' delta. Vtables without RTTI locators run together, so the slot check
  is bounded by the next referenced data object.
- Result: all 197 exe constants/evidence addresses map; functions moved ~+0x170..+0x1a0, vtables ~+0x38..+0x48, but
  framework B's title/number text vtables were REORDERED (0x31c2b0 -> 0x31c468, 0x31c4d0 -> 0x31c3a8; verified slot by
  slot). InGameUI::HandleKeyAction is the one changed function: only its inline switch tables differ (index table
  byte-identical, targets uniformly +0x170), so the action ids hold. The 18 signatures match at the new RVAs.
- World layer: ctors of Character, Player, ControllerPlayer, Item, ItemEquipment, Skill, SkillManager, CombatManager,
  Npc, Destructible, Engine, Entity, Region, World, WorldCamera, GameCamera, HotSlotOptionSkill are unchanged.
  Changed layouts: GameEngine (grew at ~+0x3e48 and ~+0x36c00; our market map +0x40e0 -> +0x40f8, dev only), SkillBuff
  (+0x10 from 0x5e4; the buff-self subclasses follow; we read no raw SkillBuff field), Pet (new member +0x53b0).
  vtcmp: Skill, Character, Player, Monster, HotSlotOption(Skill) slot-identical (Pet only overrides the new
  GetDefenseAttributeCap); the ??_7 exports of the Item family are one folded table and say nothing (Item+0x4c8 is
  self-checked at runtime against the exported GetDefenseAttributes overrides).
- Live (dev character, Devil's Crossing): gate accepts 1.3.1.1, `exe_ui` layout ok, main menu + character picker +
  Start, vitals, NPC review, inventory and skills windows through HandleKeyAction. Not verified: synthetic Ctrl+C did
  not open the inventory and Tab from a window's column back to its tab row did nothing (navigator side, not game
  data; may predate the patch -- needs real keys).

## 2026-10-12: Shift+W/A/S/D dropped in text fields (issue #4)
The game-key filter in `app.cpp` applied the world's chord rules (Shift+W/A/S/D = free cursor, Ctrl+digit = quickbar
read) on every screen, after the screen's `passes_key`, so an edit session's capital W/A/S/D press was swallowed
("DELETE" typed "ELETE"). `Screen::passes_key` now takes the event's released/shift/ctrl flags and both rules live in
`InGameScreen::passes_key`; the filter just asks the current screen. Reported with a verified alternative fix by
ohylli (gate on `captures_raw_input()`); this version scopes the rules to the screen that owns them instead. Not
re-tested live.

## 2026-10-12: dropping items (Delete in a bag)
Requested by testers. Read statically: the game's release of a held item over the world is
`CursorHandlerItemMove::ActivateWorld` (Game.dll+0x1845f0). A held item is already off the grid; a soulbound (+0xc00,
`Item::IsSoulbound`) or untradeable (+0xc02, `IsUntradeable`) item gets dialog 0x15 ("destroy?") and on Yes
`SendRemoveItemFromInventory` (`CursorHandlerItemMove::Update`, +0x187016); anything else goes to
`ControllerCharacter::SendDropItemRandom(id)` (a `GameEngine::CreateItemCopy` at a random point in a ring around the
character + `RemoveInventoryItemConfigCmd`). The mod mirrors it from a bag row: `PlayerInventoryCtrl::RemoveItem(id,
true)` then the drop or (after our own no/yes picker) the destroy. Verified live by the user 2026-10-12.

## 2026-10-22: enemy projectiles and ground effects (src/projectiles.cpp, branch projectile-recon)
- Recon (`/projectiles?on=1`): projectiles are `ProjectileBase` entities (9 classes) found by `world::entities_near`
  (Region::GetEntitiesInSphere, one reused game vector: a fresh vector per call leaks its storage, which
  `find_entity` still does every locked frame). `Entity::GetOwner()` names the shooter (player / foe / other) for
  every one, fragments spawned at the target included. `ProjectileBase::GetTotalSpeed` matches the measured flight
  within ~5 %; `GetHitTime`/`GetMissTime` read 100/100 or 0/0 -- not times. The first movement's line of travel
  predicts the closest approach within ~1 u. Sweep ~30-160 us a frame; the spawn-time `label_of` was the likely
  15 ms spike (labels only while recording now).
- Measured: skeleton crossbow bolts 38 u/s from ~12 u = ~0.27 s from appearing to arriving (unavoidable by
  reaction, for a sighted player too); deathbolt 20 u/s ~0.8 s; Nomos Dred's lightning nova 18 orbs at 16 u/s every
  7.5-9.5 s, the "ring" telegraph ~0.35 s before the orbs.
- The cue: slow (< 25 u/s) enemy projectiles tick `assets/audio/projectile.wav` from their position every 150 ms,
  +5 st when the current line passes within 1.5 u and approaches; fast ones tick once at launch at 0.6; every enemy
  one is audible, not only the aimed ones (the user's rule: stepping from one must not walk into an unheard one).
- Enemy ground effects (sigils, landed pools) are `ProjectileAreaEffect` entities, invisible to the hazard layer
  (painted sector cells only). Every enemy one still for 2 frames is a `world::GroundZone` that `hazard_at` answers
  first, so lanes / bed / pointer sound it. **Radius** (static RE, verified live): the targeting pass (Game.dll
  0x408a00; `GetTargetsInRadius` at +0x408a88 / +0x40c576) walks a pointer vector at +0xd18/+0xd20 and uses
  `[entry + 4]` around +0x748; Elemental Seal read 3.5 = its record x3. The object's inline floats never hold it
  (0x538 / 0xde8 read 3.0 on Kilrian's Conflagration by coincidence: 20 / 5 on the Seal). One visual record serves
  skills of different radii (the sigil: 2.5 / 3.2 / up to 5.5), so a record table cannot replace the live read.
- Not covered: caster-centred auras that move with the enemy (Kilrian's Ring of Fire, `BuffAttackRadiusDuration`,
  which also auto-casts Fireblast every 0.2-0.4 s with no animation -- `/casts` NOW lines; Nomos Dred's Lightning
  Field); waves/cones stay the telegraph's (the Skeletal Monstrosity's Double Strike: two 8 u x 3->5 u waves at
  +0.60 / +1.14 s, step sideways).

## 2026-10-22 (evening): projectiles say where they are going and how they fly
- `+0x730` of every `ProjectileBase` is a WorldVec3 (Region* + Vec3 at +0x738) = the point its cast was aimed at. A
  search for the cast's aim found it there for arrows, fireballs, grenades and a sigil; the probes that "missed" had
  their aim near a 128 u region edge and the search used the aim's region, not the projectile's (read whole and
  converted with `world::world_point`, the grenade matched within 0.8 u). A fragment (Rolderathis's ice spikes, burst
  out of his ice grenade) stores its own launch point: no aim.
- How a projectile flies is its class's movement code tuned by its record, and the deciding number is live:
  `launchAngle` (+0x578, `GetLaunchAngle`). Above zero = thrown in an arc that lands at the destination (Zarthuzellan's
  `ProjectileExploding` fireballs 35, ended at the aim point; ice grenades 25); zero = straight, on through the aim
  point to its range (+0x538; the deathbolt, `FireballLike`). The record survey by class: ArrowLike / FireballLike /
  TerrainFollowing are speed + range (+ explodeOnMiss); Grenade / Exploding carry launchAngle, bounce physics and
  `useTrajectory`; Homing / Boomerang (`projectileFollow`), Orbiting, Spark, Chain, Inverse, Fragmenting are a handful
  of records each, left on the straight rule until one is met.
- The projectile cue: a lobbed one ticks high when the player stands within 2.5 u of where it lands; straight ones keep
  the line + range rule. `tools/gen_telegraph_records.py` marks every aimed projectile skill whose projectile record has
  a launch angle as "area" at cast start (132 records: Zarthuzellan's fireball, Rolderathis's freeze blast, molotovs,
  acid spits, ice bombs); rings keep "ring".
- The recording persists to `%LOCALAPPDATA%\Grimdark\projectiles.log` (appended, a header per session) and starts on
  in dev launches: an evening of probe lines lived only in the game's memory and was lost when the game closed.

## 2026-10-23: ground waves tick like projectiles
- The user met Voldrak, the Destroyer (Smuggler's Pass): his Groundbreaker (`Skill_AttackWave`) said "wave" at cast start
  and was silent after. It makes no ProjectileBase. A cast-window probe (`/projectiles` recorder: every non-projectile
  entity new in the 4 s after a foe wave cast, WNEW / WGONE) caught all four casts: an `EffectEntity` with the skill's
  `lineEffectName` record (groundripper_fx01), owner 0, appearing ~1.1 s after the cast start and travelling 7-14 u in
  ~1.4 s (6-10 u/s); the one that came within 0 u hit (impact fx at the player). The rest of the window was streaming
  scenery (Decoration, Decal, FireLight).
- A survey of every non-player skill's fx fields: projectile classes (~550 records) are already ProjectileBase; radius
  blasts / radius lightning / chains (~260) put their effect where and when the damage lands (no lead time: the
  telegraph is their warning); `Skill_AttackWave` (151) is the one family whose effect travels -- 93 name it
  (lineEffectName 77, fxPakName 16; 25 distinct effects). Beams are a handful, left alone.
- `tools/gen_wave_effects.py` -> `src/wave_effects.h` (skill -> effect). A foe's wave cast (telegraph::on_cast, before the
  telegraph's own mode/shape gates) arms its effect for 3 s; a new entity with that record in the sweep is tracked as a
  slow projectile owned by the caster (straight-line accent, 2.5 u zone; no ProjectileBase reads). Ownerless, so the
  cast is the attribution: groundripper_fx01 is also a player relic's beam groundEffect.

## 2026-10-28: building on an old Windows SDK (fork)
- A Build Tools 2022 install with only SDK 10.0.20348 failed in `src/tts_onecore.cpp`: that SDK's C++/WinRT (2.0.200609)
  includes `<experimental/coroutine>`, an error under MSVC 14.44 /std:c++20. CMake now checks the SDK's `base.h` for
  `#include <coroutine>`; when absent it downloads Microsoft.Windows.CppWinRT 2.0.240405.15 (SHA-256 pinned), runs
  `cppwinrt.exe -input <sdk>` into `build/ninja/cppwinrt/include` and adds that to tts_onecore.cpp only. Built clean,
  gdcore_tests 150/150, the WinRT imports stay delay-loaded (the newer headers no longer import winrt-string at all).
  Not run in the game yet.

## 2026-10-28: controller support (fork; src/gamepad.cpp, src/gamepad_input.cpp, src/core/gamepad.*)
- Static RE (1.3.1.1, disk): the game's pad reads are DirectInput.dll `ProcessJoystickEvents` -> xinput1_3 ordinal 2
  (slot 0, one call site, 4 s reconnect after a failure, connected flag device+0x334) and Engine.dll
  `SteamControllerDevice::Update` (only caller `Steamworks::Update`; connected flag device+4, events device+0x10). What a
  pad changes is `GameEngine` input mode +0x37748 (exported Get/SetInputMode; Game.dll never calls the setter) -- read by
  `DefaultRequestSkillAction`, `MoveAndPickUpItem` (now 0x162930; the RVAs in docs/re_*.md are 1.3.0.8's), `HotSlotOptionEvade::Activate`,
  `HotSlotOptionSkill::Activate`, `Player::FindPath`, `CompareItems` and the UI text generators. Hooked: the xinput call
  answers not connected, the Steam device never updates, `ControllerIsConnected` is false, `SetInputMode` stores 0 (a
  nonzero request is logged, five times at most).
- The mod reads pads itself: XInput 1.4 (empty slots probed every 2 s) + DualShock 4 / DualSense over HID (overlapped
  reads on a thread per pad, report offsets as in d4-assist / hid-playstation, a 3 s scan), OR-merged so a pad mirrored by
  Steam / DS4Windows cannot press twice. Layout logic is engine-free and unit-tested (tests/gamepad_tests.cpp, 18 cases).
- Built clean, gdcore_tests 168/168. Not run in the game yet.

## 2026-11-01 -- fork identity: Grimdark Controller (Berenion/grimdark)
- Upstream declined the controller work, so this repo is a fork that merges upstream/main as it moves. The build's identity
  is compile-time env with upstream's values as defaults: GRIMDARK_REPO (update check + installer release list),
  GRIMDARK_APP_NAME (install folder, shortcuts, Add/Remove key, spoken update line), GRIMDARK_INSTALLER_EXE. CI sets them
  in build.yml's top-level env (repo = github.repository); the repo-root .cargo/config.toml sets them for local cargo builds (root, not installer/: cargo searches it from the CWD).
  %LOCALAPPDATA%\Grimdark (log + settings) stays shared with upstream: settings.cpp rewrites every key it loaded, so
  neither build drops the other's keys.
- Fork tags are v<upstream>-pad.N; core/update_check now compares semver pre-release identifiers (it used to ignore the
  suffix, so pad.1 vs pad.2 compared equal). Never push upstream's tags here: a v* tag push publishes a release.
