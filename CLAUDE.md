# Grimdark — screen-reader accessibility mod for Grim Dawn (C++)

Injected DLL that hooks the game's own engine exports. Design lineage: `../wotr-access` (C#/Unity sibling
project by the same author) — see `docs/design-notes-from-wotr.md` for the decisions we carry over.

## Game facts
- Grim Dawn v1.3.1.2 (x64), Steam build (relocated from 1.3.0.8 on 2026-10-08, from 1.3.1.1 on 2026-11-19). Install: `C:\Program Files (x86)\Steam\steamapps\common\Grim Dawn`.
  The 64-bit game is `x64\Grim Dawn.exe`; **its working directory must be the install root** (where
  `database.arz` and the `.arc` archives live) or it starts with no data and crashes in a string compare.
- Closed-source C++ (Titan Quest engine lineage), split into DLLs that **export named, MSVC-decorated C++
  symbols**: `Engine.dll` (6283 exports), `Game.dll` (25100 -- note pefile silently caps at 8192 unless
  `max_symbol_exports` is raised; `tools/gen_exports.py` does), `DirectInput.dll` (34, Crate's own wrapper
  around dinput8), `Widget.dll` (tool UI, irrelevant). The exe itself is SteamStub-packed on disk (static
  analysis of it is useless; it is plain in memory) and holds the in-game UI screens (MenuManager, UI*Window).
- Settings: `%USERPROFILE%\Documents\My Games\Grim Dawn\Settings\options.txt`. `screenMode`: 0 fullscreen,
  1 windowed (title bar), 2 borderless. `inactiveUpdateRate = 0` means the engine stops ticking when the
  window is unfocused (our per-frame hook stops too). We run windowed 1600x900.
- Rendering text: bitmap fonts via `GraphicsCanvas::RenderText2d*` (Engine.dll, exported) — the (x,y)
  overloads are called by the Rect overloads, so only the (x,y) family is captured.
- Input: the game polls `DirectInputDevice::GetNumKeyEvents()/GetKeyEvent(i)` each frame. `Display::HandleKeyEvent`
  exists but the exe's UI does not route through it. Returning 0 from `GetNumKeyEvents` mutes game keys.
  Key codes are the game's `InputDevice::Button` enum: DIK scancodes for plain keys, then a contiguous
  block for extended keys -- Home 0x78, Up 0x79, PageUp 0x7a, Left 0x7b, Right 0x7c, End 0x7d, Down 0x7e,
  PageDown 0x7f, Insert 0x80, Delete 0x81; Ctrl 0x1d, Right Ctrl 0x6b, Alt 0x38, Right Alt 0x76, Shift
  0x2a/0x36; mouse buttons from 0x91 (wheel up). Full table from the game itself: `tools/exports/keynames.txt`
  (dev route `/keynames`).
- Navmesh is Recast/Detour (`GAME::NavManager`, `CROWD::` = DetourCrowd); `NavManager::FindPath` is exported.
- The exe's main loop (exe+0xee4d..0xef91, read 2026-08-21) per iteration: `display->Update(dt)` (vtable+0x28 on the
  object at mainobj+0x250 -- the engine's `Display` in the menus, the exe's own subclass in the world, whose
  override never reaches the exported `Display::Update`), the input device poll, `SoundManager::Update`,
  `PresentSurface`, `Steamworks::Update`, `Engine::Update(0,0,0,0)`. **Our per-frame tick is the `Engine::Update`
  hook** (runs after the poll and the render in both states); `Display::Update` is only counted. A second path
  (loading screens) calls neither.
- In the world the HUD's numbers ("250/250"), "Active Quests" and the game's message boxes ("Okay") ARE captured
  by the RenderText2d hooks; the intro cutscene subtitles and the minimap's area name are drawn another way.
- Reaching in-game objects (plan): `GameEngine` is not an exported singleton -- capture `this` by hooking a
  per-frame member (`GameEngine::Update(int)`, `ControllerPlayer::Update(int)`), `Singleton<NavManager>::Get`
  is exported, `GameEngine::GetMainPlayer()` / `PlayerManagerClient::GetMainPlayer()` give the `Player`;
  positions are `Entity::GetCoords()` / `Character::GetFootCoords(bool)`, `WorldVec3` = region-relative Vec3 +
  `Region*` (`GetWorldPosition`, `SetFromWorldPosition`, `PutOnFloor`); probes: `NavManager::IsPointOnPathMesh`,
  `IsBlocked(Vec3,float)`, `FindStraightMovePoint`, `FindClosestPointOnPathMesh`, `World::GetEntitiesInSphere`,
  `World::GetIntersection(WorldRay)`; `Character::GetCurrentAttackTarget`, `GetMovementTarget`.
- Measured 2026-08-21 (src/world.cpp): `WorldCoords` = `Region*` (+0) + origin `Vec3` (+8) + pad (+0x14) + 3x3 axes
  (+0x18, size 0x40; read from the ctor 2026-08-22);
  `Coords` = 3x3 axes then origin (offset 36); `WorldVec3` = `Region*` + `Vec3` (region-relative; region 0A001's
  origin coincides with world). Player "Bob" spawns at world (59.2, 7.8, 97.2) in `Levels/Region0A001.lvl`;
  default camera yaw 0.8727 rad. **WASD is the game's own feature**: `movementType = 1` in options.txt (the
  launcher does not set it; 0 = click-to-move). Screen-up in world xz = (-sin yaw, -cos yaw), screen-right =
  (cos yaw, -sin yaw). `NavManager::IsPointOnPathMesh` on `WorldVec3::PutOnFloor`'d points gives the wall
  probes (free distance 3..15 units at the spawn). Own audio: miniaudio mixer in src/audio.cpp (the dev
  launcher's process mute silences it too).
- Entities around the player: `Region::GetEntitiesInSphere(mem::vector<Entity*>&, const Sphere&, bool, EntityListType)`
  on the player's `Region*` (NOT `Level::` -- a Region holds its Level at +0x68 and its World at +0x30; calling
  the Level variant with a Region crashed). Read with `tools/dll_dis.py Engine.dll <name>`: `mem::vector` is
  std-like `{begin, end, cap}` and is appended to; `Sphere` = `{Vec3 centre, float radius}` in the calling
  region's coordinates. Keep the radius modest (25 worked, 400 crashed the game inside the query).
  `Object::GetObjectName()` returns `const char*` (the .dbr record path), `GetObjectId()` the id the
  controller APIs take. Dev routes run under an SEH guard (`route faulted: ...`); hangs are not caught.
- **Targeting = the virtual cursor** (verified 2026-08-21): the exe re-resolves its combat enemy from the cursor
  every frame (`SetCombatEnemy(0)` at exe+0x218eb then `SetCombatEnemy(id)` at exe+0x21b9c), so
  `world::lock_target(id)` just keeps the cursor override on the entity's projected position
  (`WorldCamera::Project(WorldVec3 + 1.0 y, Viewport(0,0,w,h))` -> Vec2 via hidden pointer) each frame, and the
  game hovers, names ("Training Dummy" + level in `box_font` HUD text), targets and attacks it natively. Off-screen
  entities are handled by **direct aim** (2026-09-23, `world::direct_aim_apply`): skill requests read the controller's
  combat enemy +0x468 (+0x46c is the ALLY) and a hot slot fires at the mouse repeat data (+0x43c id, +0x440 WorldVec3),
  so the `HotSlotOptionSkill::Activate` and `HandleActionFromMouse` hooks write / substitute the locked target and the
  game's own range, walk and clamp rules do the rest. The cursor override stays: the exe drops a mouse event whose
  pick finds nothing.
  Dynamic class names: `Object::GetRTTIClassInfo` dispatched through the vtable slot found in `Object::vftable`
  (the export is the base implementation); `RTTI_ClassInfo` = vptr + `const char* name` ("Player", "Npc",
  "Monster", "PlayerSpawnPoint"). `/entities`, `/project?id=`, `/lock?id=|off=1`, `/target` are the dev routes.
- Hot reload crashes (the in-world `bad_function_call` abort of 2026-08-21 and a main-menu crash inside our
  tick on 2026-08-22) were Detours transactions updating only the calling thread: `grimdark_unload` runs on a
  remote thread, so the game thread could sit in a trampoline being freed (and the new DLL maps at the old
  base, which is why the stale frames symbolized against the new PDB). Fixed: `hooks.cpp ThreadUpdater`
  suspends/updates every thread of the process in every transaction, and the unload sleeps 250 ms after
  detaching so in-flight hook bodies finish before FreeLibrary. Verified: 3 reloads at the main menu and 2 in
  the world. `tools/windows.py` reads the text of a modal dialog the unfocused game put up.
- Combat feedback (static RE 2026-08-22, `docs/combat-feedback.md`): floating numbers/Miss/Dodge/Block are
  `EventManager::Send(ev, 0x1b)` from `CombatManager::TakeAttack` (text at ev+0x40, u16); all damage runs
  through exported `CombatManager::ApplyDamage(float, PlayStatsDamageType const&, CombatAttributeType,
  vector<attackerId>)` on the victim; no player combat log exists (`gLogCombat` is a dev printf switch);
  popups = `ControllerPlayer::SetUserText(tag)`, banners = `GameEngine::AddUINotification`.
- Positional voices (2026-08-22, verified through the loop): `src/tts_onecore.cpp` (own OneCore backend via
  C++/WinRT, the only TU including `winrt/*`; worker-thread MTA only; api-ms-win-core-winrt-* delay-loaded so a
  missing runtime loses combat speech, not the mod) -> `src/voice.cpp` (Mark = at the enemy, Zira = the
  player; worker queue, per-text PCM cache, `audio::play_pcm` with groups/replace; own ring at `/voice`,
  `/voices` status; falls back to `speech::speak` tagged "(prism)") -> `src/combat.cpp` (the `EventManager::Send`
  0x1b hook, `core/combat_text` parse, `core/combat_coalesce` 150 ms same-place merge, `core/threshold_watcher`
  10 % health steps). H = vitals, menus/UI and every other KEY stay on `speech::speak` (prism/NVDA): the voices are for what happens in the world, keys are screen-reader readouts unless noted. Synthesis is 4-15 ms per line.
  Dev: `/voice?say=&voice=mark|zira&pan=&replace=1`, knobs `vol= coalesce= window= cap= max= near= far= floor=`,
  `/combat[?raw=N]`. Voice level: lines are peak-normalized (0.8), bypass the 0.6 master, and use their own
  rolloff `world::voice_gain` (1.0 out to 9 units = moderateRange, linear to 0.4 at 32 = bossRange); the review
  pings keep the sonar curve. The game's range table is `records/game/gameengine.dbr` (`tools/arz.py` reads
  database.arz): meleeRange 1.25, meleeTargetDistance 2.4, shortRange 4.75, moderateRange 9, longRange 15,
  maximumRange 18, bossRange 32, camera 20..48; skills pick one by `distanceProfile`.
  Known: the first `/keydown` hold right after a hot reload did not register once (second try worked).
- Tooltips/descriptions are exported as data: `Item::GetUIDisplayText`, `Monster::CreateUISummaryText`,
  `Conversation::GetText`, `Quest2::GetText`, `LocalizationManager::GetText(tag)`.
- Movement/targeting substrate (all `GAME::ControllerPlayer`, exported): `SetControllerDirection(Vec3)` +
  `SetControllerMovementLength(float)` + `HandleActionFromJoystick(WorldVec3, bool)` is the gamepad/WASD
  movement path; `SetCombatEnemy(id)`/`GetCombatEnemy`/`FaceTarget(id)`/`SetTargetPoint(WorldVec3)` is
  targeting state; `SendSkillAction`, `InstantSkillAction`, `InteractAction`, `NpcAction`, `ItemAction`,
  `UseItem` take explicit world points / entity ids. `options.txt` `targetLock` is the game's own
  target-lock option. To be verified in-game before the movement/targeting design.

## The NVDA-killing bug and its fix (solved 2026-08-21)
`DirectInput.dll` contains a verbatim copy of Microsoft's old "Disabling Shortcut Keys in Games" recipe: on
every focus gain it installs a `WH_KEYBOARD_LL` hook (`DirectInputDevice::LowLevelKeyboardProc`, whose body is a
pure `CallNextHookEx` pass-through) and calls `SystemParametersInfo(SPI_SETSTICKYKEYS/TOGGLEKEYS/FILTERKEYS)`; on
focus loss it unhooks. The hook churn during the busy focus transition got NVDA's low-level hook timed out
and silently removed by Windows (symptoms: NVDA keys dead after alt-tab; CapsLock/OCR blocked; restarting
NVDA fixed it until the next focus change). Fix in `src/hooks.cpp`: in-process detours on
`SetWindowsHookExA/W` refuse `WH_KEYBOARD_LL` with a fake handle, `UnhookWindowsHookEx` accepts it, the three
SPI set calls are swallowed, and any hook already installed at injection time is removed. Verified: NVDA
survives focus changes, OCR works, mod speech works.

## The dev loop (working as of 2026-08-21)
Everything runs with the game **visible but never focused** and muted, so iterating never touches the
developer's screen reader. Client: `uv run tools/gd.py <cmd>` (add `--with pillow` for `shot`).
- `gd.py launch [--speak] [--nobuild]` — build, start the game unfocused with the DLL injected before init, wait
  for `/health` (45 s; a crashed instance is killed, not left in its crash-reporter dialog).
- `gd.py health | text | speech [--since N] [--wait S] | log [--since N] | buttons`
- `gd.py say "…" | mute on|off | gamekeys on|off | key enter | key a --ch a --shift | keys "text"`
- `gd.py click X Y [--button right] | mouse TYPE X Y | cursor X Y | cursor --clear | kill`
- **Player launcher = `gdlaunch.exe`** (2026-09-18, `src/launch/main.cpp`, shares `src/inject/inject_common.h` with
  gdinject; verified live): finds the game via HKCU SteamPath -> `libraryfolders.vdf` -> `appmanifest_219990.acf`
  installdir (`--game` / `game_path.txt` override), refuses a non-x64 exe, Steam not running or the game already
  running with a MessageBox (not prism: the mod is not up yet), fakes GDCommunityLauncher's Steam environment block
  (SteamEnv/SteamClientLaunch/SteamGameId/SteamAppId/SteamOverlayGameId/SteamAppUser/SteamUser/STEAMID/USER_MYDOCS)
  so the stub does not relaunch, injects at suspended start, then keeps the console open tailing the log's
  `[speak]`/`version:`/`grimdark:`/`crash:`/`speech:` lines until the game exits. Honours GRIMDARK_NOFOCUS for the
  initial show state, so `GRIMDARK_NOFOCUS=1 GRIMDARK_MUTE=1 GRIMDARK_PORT=8791 gdlaunch.exe` is a dev-safe run of
  the real launcher. Deliberately NOT Steam launch options (Factorio Access support-load lesson).
- Hot reload: `powershell -File tools/inject.ps1` (ejects via the DLL's exported `grimdark_unload`, rebuilds,
  re-injects). Never tear down from DllMain (joining a thread there deadlocks under the loader lock).
- How it stays unfocused: `gdinject --launch` uses `STARTF_USESHOWWINDOW` + `SW_SHOWNOACTIVATE`, and with
  `GRIMDARK_NOFOCUS=1` the DLL defangs the game's own `SetForegroundWindow`/activating `ShowWindow`/
  `SetWindowPos` and fakes `GetForegroundWindow`/`GetActiveWindow`/`GetFocus`. Do NOT un-minimize or restore the
  window from outside — the game activates itself on restore. `inactiveUpdateRate` must be nonzero
  (launcher patches 0 -> 30) or the engine stops ticking while unfocused.
- Input reaches the game through its own device poll (`DirectInputDevice::GetNumKeyEvents/GetKeyEvent`,
  `GetNumMouseEvents/GetMouseEvent`, hooked): synthetic events are appended to the real ones, one "frame
  group" per `Display::Update`. The cursor is overridden in both `GetCursorPosition` (screen variant
  converted with `ClientToScreen`) and raw `user32!GetCursorPos`. Measured layouts are in `src/hooks.h`.
  **MouseEvent types: 1 = left down, 9 = left up, 2 = right down, 10 = right up, 3..8/11..16 = buttons 3..8,
  17/18 wheel, 0 = per-frame idle with position.** Verified: a click on the main menu's "Create" opens the
  Create Character dialog.
- The engine dispatches events to `Engine::AddWidget` registrants (vector at Engine+0x3e8, vtable +0x10 key /
  +0x18 mouse). The exe's real mouse handler is `exe+0xbef10`; several slots are import thunks or
  `return false` stubs — **never detour those** (it crashed the game; `GRIMDARK_HOOK_WIDGETS=1` re-enables the
  experimental handler hooks). The exe is SteamStub-packed on disk: `tools/dump_exe.py` dumps the unpacked
  image from the live process and `tools/exe_dis.py <rva>` disassembles it with Engine/Game export names.
- **A crashed game looks alive**: its crash handler keeps the process in a dialog, `tasklist` still lists it and
  `/health` answers "game thread did not run the job". `uv run tools/gd.py status` decides running / CRASHED
  (dialog or crash child process) / hung; run it whenever a dev call times out, before assuming a hang.
  `gd.py kill` kills only pids whose image is exactly `Grim Dawn.exe`; `tools/stacks.py` refuses any pid that
  is not the game (a stale pid can be reused by another process, and the walker suspends its threads).
- Crash triage: `uv run --with pefile tools/stacks.py` (stacks + faulting registers) while the crashed
  process is still alive. Read the cause before relaunching. `tools/dll_dis.py <dll> <name>` disassembles an
  Engine.dll/Game.dll export from disk (they are not packed) with calls annotated -- read the callee before
  guessing a struct layout (that is how Sphere = {Vec3, float}, mem::vector = {begin, end, cap} and
  "Region::GetEntitiesInSphere, not Level::" were established). `Object::GetObjectName` returns `char const*`.
  In dev routes that walk game objects, read each one under a `__try` guard (src/world.cpp `read_entity`).

## Where things stand (2026-09-20)
- Playable end to end: main menu -> character creation -> the world. In the world: WASD (the game's own), wall tones,
  the sonar field, the review cursor groups (. n b m c ] [ + Shift/Alt), J / I / Enter / U interaction on the locked
  target, rooms + painted area names, riftgate travel, the Ctrl+M map (markers + shrines), every in-world window
  (inventory, skills + devotion, codex, factions, vendor incl. faction tiers, stash, crafting, inventor, loot filter,
  quest reward, shrines, pets), combat voices (Mark at the enemy, Zira = you), telegraph cues, harmful ground, the
  T / Ctrl+T settings overlays, the F1 mod menu. Shipping: `gdlaunch.exe`, the Rust installer, CI releases with a
  rolling `ci-latest`, the version gate.
- Not modelled: key rebinding, the Multiplayer / Network screens, the Illusionist (transmute), the inventor's Convert /
  Reroll tabs, a remote stash, attribute reclaim, aiming at open ground for point-target skills (runtime target type 4).
- Open threads: sonar tuning by ear (the stagger rework is kept, the crowd compression was rejected by testers), the
  telegraph ramp-to-hit, real-key Ctrl-chord dispatch (synthetic keys cannot verify it), rooms authoring for the
  remaining regions, the game-patch relocation tool (needs a second exe build to exist first).
- The dated history of every piece above is `docs/devlog.md`; mechanisms are in `docs/*.md`. **Append new dated notes
  to the devlog, not here**; add to this file only a durable fact, rule or trap.

## Game patches (quantified 2026-08-25)
- Exports (367, by decorated name) survive a game rebuild unless a signature changes and degrade per feature;
  Engine/Game object offsets (~55 raw field offsets + mirrored layouts and ~6 hardcoded vtable slots, inventoried
  2026-10-08) survive unless the class changed and fail SILENTLY; the exe layer (19 RVAs + ~75
  offsets in `exe_ui`, 14 byte signatures checked by `available()`) dies on ANY relink of the exe -- every menu and
  window, deterministically -- while the export-driven world layer keeps running. Since 2026-09-18 the version gate
  refuses an unknown build outright, so a patch is "the mod is off" for players, not a crash.
- Procedure on a patch (first run 2026-10-08, 1.3.0.8 -> 1.3.1.1; `tools/exe_reloc.py`, docs/devlog.md): launch with
  `GRIMDARK_ANY_VERSION=1`, dump the exe (`tools/dump_exe.py`), `gen_exports.py` + `gen_names.py` (a changed hooked
  signature fails loudly), then `uv run tools/exe_reloc.py - - --scan <every src file with exe+0x>`: functions are
  matched by normalized body + vote propagation over .pdata, data/vtables through the refs of matched pairs, every
  `exe+0x` evidence instruction is re-checked at its new address and every vtable slot by slot. Clean -> `--apply`
  on the same files (rewrites constants + evidence in one pass, re-checks the byte signatures). The world layer:
  `exe_reloc.py <old dll> <new dll> exports|xdiff <regex>|vtcmp <regex>` -- a class whose ctor is unchanged kept
  its layout; check the hardcoded vtable slots with vtcmp. Ctors to xdiff (the classes whose raw fields we read; the
  2026-10-08 list plus later additions): Character, Player, ControllerPlayer, Item, ItemEquipment, Skill, SkillManager,
  CombatManager, Npc, Destructible, Engine, Entity, Region, World, WorldCamera, GameCamera, HotSlotOptionSkill, GameEngine,
  Pet, ProjectileBase (+0x730 destination WorldVec3) and ProjectileAreaEffect (+0xd18/+0xd20 blast-radius vector,
  entry +4; both in `src/projectiles.cpp`, 2026-10-26). A class added to the raw reads goes on this list. Then replace the row in `src/game_versions.h` (one build
  at a time: the old build lives in git + the archive), test live, and archive (`tools/archive_build.py`). Never skip archiving a build the mod works on.

## Traps and lessons (details in docs/devlog.md and the doc named)
- Pointers: never hold a `GraphNode*` or a game entity pointer across frames; hold ids (`ControlId`, object id) and
  re-resolve in the frame you use them. `casts.cpp` crashed a tester's VM reading a freed caster's vtable minutes later.
  `world::rtti_of` refuses vtables outside the exe / Engine / Game images.
- Never `class_name()` / `rtti_of()` a `Region*`: its cached RTTI slot is the virtual destructor (it destroyed the live
  region). Never sweep `Region::IsUnderground` (calls LoadLevel). Never move the player with `Entity::SetCoords`
  (stalls the controller, later crashes); `Character::TeleportToLocation` is the game's own path.
- `NavManager::IsPointOnPathMesh` is a bounding-box test, not containment: never base a player-facing decision on it
  (the wall tones use the navmesh raycast). `NavManager::FindPath` snaps its target within the radius and reports a
  complete path to the snapped point: gate exits by floor height and reach (`docs/rooms.md`).
- `World::GetRegionContainingXZ(from, x, z)` takes x, z RELATIVE to `from`.
- The live navmesh covers only the streamed-in regions (~250 u around the player, 2026-09-28): farther targets need
  the offline rooms data. `FindClosestPointOnPathMesh` returns 1 = found, 2 = nothing (out param left garbage). Fog of
  war is per region, 8-u cells, > 150 = fogged; `Region::GetFogOfWar(false)` works on unloaded regions too (lazy load
  from the FOWManager) (`docs/devlog.md` 2026-09-28, `/fogmap`, `tools/fow.py`).
- Hooking: a base export that is a COMDAT-folded stub is shared by ~2000 symbols; detouring one kills the game at
  start (`hooks.cpp` refuses bare stubs and duplicate targets). Never detour the exe's widget-handler vtable slots
  that are import thunks or `return false` stubs. Detours transactions must update every thread (`ThreadUpdater`).
  Never tear down from DllMain.
- Item handling: never hand an item to a game control the exe itself gates by class or state without mirroring the
  gate first (`UseItem` destroyed a crafting material; `is_usable`). Never `PlayerInventoryCtrl::AddItem` a split clone
  (merges back and is destroyed). `EquipmentCtrl::RemoveItem` only detaches: AddItem to the bag first. A by-value
  `std::string` argument is destroyed by the CALLEE (MSVC x64). The game validates skill reclaims only by greying the
  icon: `can_reclaim_skill` replicates the whole gate. `SendCreateArtifactCmd` validates nothing: press the window's
  Combine instead. `EquipmentCtrl::SmartAutoInsert` returns true WITHOUT placing a two-hander over weapon + off-hand
  (two displaced ids): the caller swaps (`swap_in_two_hander`); trusting it lost the weapon.
- Signatures / layouts: a byte signature longer than the 16-byte check buffer killed the game (`exe_ui` clamps);
  `push rdi` carries a 0x40 REX prefix in this exe. Our screen's tab and the game's window tab are independent: never
  key a read on the game's current tab (the one-class spirit guide bug). `WindowScreen::add_tabs` with no tabs throws
  and takes the game down.
- Input: modifier state comes from the game's per-event flags, never our own down-tracking (alt-tab leaves Alt
  "held"). Ctrl+<digit> chords must swallow the digit while Ctrl is held. Synthetic `/key` events take the real keys'
  path since 2026-10-05: the mod records them and the game sees only what the game-key filter passes, so drive the
  mod with plain `/key` as a player would (before, a dev P also hit the game's pause and silently broke L).
  The game's key enum is NOT DIK above F10: F11 0x55, F12 0x56 (`tools/exports/keynames.txt`). A mouse-button
  transition delivered off-window is lost and the exe's held byte sticks, muting WASD (`world::mouse_key` presses
  only where there is an on-screen point). A press whose projected point lies on the HUD clicks the HUD.
- Dev loop: a crashed game looks alive (`gd.py status`); hangs are not caught by the SEH guard; the dev server serves
  one request at a time; git bash rewrites `/player`-style paths unless `MSYS2_ARG_CONV_EXCL="*"`; a hot reload in the
  world can leave the game paused (`/pause?set=0`); the first `/keydown` after a reload may not register; the linker
  cannot overwrite a loaded DLL (reload = eject first). Dev routes that outlive the 8 s job timeout must own their
  state. The main menu preselects the user's last-played REAL character: check `/ui?chars=1` before Start.
- Audio: cue levels are matched by ear, not by meter (the dB(A) match made set 2 too quiet); a per-id pitch shift on
  cues flanges in a pack (rejected); mean power of a staggered pulse train = sum gain^2 / period (incoherent), but
  below ~5-10 pulses/s the ear hears per-pulse level, not mean power. Sound files live in the repo, never referenced
  from outside it.
- Telegraph shapes are a table by EXACT skill class name (`telegraph::shape_of`); the class vocabulary composes
  ("AttackRadius" is inside auras, rains and on-hit novas), so never match substrings. An unknown class is silent and
  counted in `/telegraph`, never guessed; add it to the table with its reaction.
- Game data: `Skill::GetMasteryLevel`-style accessors search live lists -- never hardcode the default-attack id;
  `GenerateUISkillText`'s int is the reclaim cost, not a level; affinity is not saved (derive from constellations);
  the DLC maps replace `world001.map` wholesale (two rooms dbs); the game's north is NOT the mod's yaw 0
  (`docs/compass.md`); Lua `Game.TeleportPlayer` is the riftgate fade, not a dev teleport.
- Rooms authoring: run `subregions` BEFORE `describe` (the suffix dedupe is per sub-region; describing first numbers
  region-wide and the sub-regions then straddle it), and after any rehome / resegment run `author.py retitle --write`
  on BOTH working dbs, then `rooms_pack.py pack` each world. Never re-tag to fix titles: the pipeline does not
  reproduce them. The dbs are build products since 2026-09-20 (`data/rooms/` is the source; never commit a db).
- Alt+Enter (`WinWindow::OnToggleFullscreen`, the game's own toggle) crashes the vanilla game in the world when the pause
  menu's Options was never opened (the InGameUI rebuild restores Options tab -1 unchecked, exe+0xcd300): the export is
  refused in `hooks.cpp`. Any display-mode change in the world goes through that rebuild; never trigger one.
- Process: every player-facing KEY goes in README.md and docs/controls.md in the same change. Ask before launching or
  driving the game when the user may be at the keyboard. Archive a game build the mod works on before Steam patches.

## Build / run
- Toolchain: VS 2022 Community (MSVC 14.44), Ninja (from VS), CMake. `tools\vsdev.cmd <cmd>` runs a command
  inside `vcvars64`. `tools\build.cmd` configures (first time) and builds into `build\ninja\`.
- `powershell -File tools/inject.ps1 -Launch` — build, then start the game with the DLL injected before it
  initializes (sets `SteamAppId` so the Steam stub does not relaunch; follows a relaunch if it happens).
- `powershell -File tools/inject.ps1` — hot reload into the running game: ejects the old DLL (so the linker
  can overwrite it), builds, re-injects. `-Eject` unloads. `-NoBuild` skips the build.
- Log: `%LOCALAPPDATA%\Grimdark\grimdark.log` (truncated on each load). Speech lines are logged as `[speak]`.
- The dev server is OFF by default (2026-09-18): it starts when settings.txt has `devserver=1` (F1 -> mod options,
  `screens/mod_options.cpp`, the row shows the live state) or `GRIMDARK_PORT` is set, which `inject.ps1 -Launch`
  always does, so the dev loop is unchanged. `gd.py status` reports HUNG when the server is simply off. The CRT is
  static (`CMAKE_MSVC_RUNTIME_LIBRARY`): no VC++ redistributable on a player's machine.
- The F10-F12 GetAsyncKeyState dev hotkeys were removed 2026-09-13 (the dev routes replaced them long before).
- Speech: prism (prebuilt SDK in `third_party/prism-bin`, delay-loaded from next to grimdark.dll).
- **The mod's version is embedded at configure time** (`GRIMDARK_VERSION` env -> `build/generated/grimdark_version.h` +
  `build/version.txt`, which `tools/package.py` ships): CI sets `v<tag>` or `ci-<short sha>`, a local build is `dev`. The
  update check (`src/update_check.cpp`, WinHTTP, `core/update_check.h`) is silent on `dev`. Changing it needs a reconfigure.
- Hooking: Microsoft Detours, vendored source in `third_party/Detours`, built as a static lib.

## Tools (Python: `uv run tools/<script>.py` -- the repo root `pyproject.toml` declares lz4/pefile/capstone/numpy/scipy/pillow, no `--with` needed)
- `tools/rooms.py` + `tools/gdmap/` (arc, map header, level bodies: navmesh tile layers + terrain layers,
  segmentation, renderer, `roomsdb.py`) -- the rooms pipeline, `docs/rooms.md`. **The rooms data is committed as
  text in `data/rooms/<world>/`** (JSONL per region + zlib'd grid blobs); `tools/rooms_pack.py` (stdlib-only)
  packs/unpacks it. CMake builds `build/ninja/assets/rooms.db` + `rooms_base.db` from it (the DLL reads those);
  the authoring tools work on `build/rooms/rooms.db` (`rooms_pack.py unpack`), and `rooms_pack.py pack` writes
  the text back after an authoring session -- commit the text, never a db. `rooms.py area devilscrossing --write`
  edits that working db; floor plans in `build/rooms/`.
- `tools/package.py [--out dist/grimdark.zip] [--version v]` — the player zip in its final layout (`grimdark/` folder:
  gdlaunch, DLL, prism, injector, assets, README, licenses, `version.txt`) + the PDB beside it. `.github/workflows/build.yml`:
  job `mod` (build -> `gdcore_tests` -> package), job `installer` (cargo test + build in `installer/`), job `publish`
  (a `v*` tag = a GitHub release with zip + installer + pdb; every main push recreates the `ci-latest` pre-release).
- **Installer** (`installer/`, Rust + wxdragon = native wx controls, modelled on wotr-access's; 2026-09-19): per-user
  install to `%LOCALAPPDATA%\Programs\Grimdark` (replaced wholesale; `%LOCALAPPDATA%\Grimdark` data untouched),
  Desktop + Start Menu `Grimdark.lnk` via IShellLink, HKCU Uninstall key `Grimdark` (`paths::APP_NAME` is the one
  name, spoken and on disk; the rebrand from GD Access / gdaccess landed 2026-09-18 before any release; Add/Remove runs the copy inside the folder with
  `--uninstall`, which re-execs from %TEMP% to delete itself), version = `version.txt` vs release tags (semver only;
  `ci-latest` listed LAST as "latest successful CI build"), Install from file, `--cli` (no Launch button: Defender
  blocks a spawn from the installer even with a folder exclusion, 2026-09-21; the shortcut is the way to play). Download/unpack on a
  worker thread, a 100 ms wx Timer drains progress into the log. Building it locally needs Ninja + CMake on PATH:
  `tools\vsdev.cmd cargo build --release --manifest-path installer\Cargo.toml` (wxdragon-sys compiles wxWidgets, ~10 min
  cold). Toolchain pinned by `installer/rust-toolchain.toml`.
- `tools/archive_build.py --version X` — copies the game's exe/DLLs + the unpacked dump to `../grim-dawn-archive/<version>-<pe-ts>/`
  (refuses a stale dump). Run once the mod WORKS on a build, so it is the baseline for the next patch ("Game patches" below).
- `tools/gen_exports.py` — dumps `.def` files and undecorated export listings from the installed DLLs into
  `tools/exports/` (regenerate after a game patch).
- `tools/gen_names.py` — resolves the exports we hook by regex over the undecorated listing and writes
  `src/gd_names.h`; fails loudly if a pattern does not match exactly one export.
- `tools/fow.py [character] [difficulty] [--region s] [--grid]` -- a character's saved fog of war (map.fow, every region).
- `tools/gen_wave_effects.py` -- the travelling effect record of every ground-wave skill (`Skill_AttackWave`: lineEffectName /
  fxPakName) into `src/wave_effects.h`, for the projectile cue; rerun after a game patch.
- `tools/gen_telegraph_records.py` -- per-skill-record telegraph shapes by arc (frontal / narrow-ring fans) from database.arz
  into `src/telegraph_records.h`; rerun after a game patch, like `gen_names.py`.
- `tools/padprobe.py [seconds]` -- the Sony pads (USB / Bluetooth HID) and XInput slots as the mod's reader sees them,
  with the first report of each id; no game needed.
- `tools/arz.py <record-path-regex> [field-regex]` — reads `database.arz` offline (records + their fields).
- `tools/telegraph_audit.py <monster-record-regex>` — a monster's skills per phase (follows `poolToSpawnOnDeath`), each with
  its class and the telegraph verdict (shape / silent / UNKNOWN) from `src/telegraph.cpp`'s table. Offline.
- `tools/stacks.py [pid|exe] [n]` — native stack dump of all threads via dbghelp; `tools/pe_survey.py`,
  `tools/dinput_hook_scan.py` — static analysis helpers; `tools/hookmon.py` — LL keyboard hook monitor.
- Reference implementations in `reference/`: `iagd` (injected Detours hook DLL for this game, MIT) and
  `GDCommunityLauncher`.

## Design rules (agreed 2026-08-21)
- **Menus one by one, no crawlers, no screen space.** Each game screen is a dedicated `Screen` subclass
  declaring its graph over OUR model of that screen (the controls it has, in player-sensible order). Identity,
  labels, state and activation come from the game's own objects through `src/exe_ui.h` (the exe's two private
  widget frameworks, reached by base-relative layout; `docs/exe-ui-layout.md`) or exported calls
  (`DialogManager` for message boxes): a button is pressed through its listeners / its window's registry,
  never by a click at a drawn label or a measured pixel. Text capture (`textcap`) is a dev discovery tool
  (`/text`) and the fallback's name source only. Unknown screens get the honest "unsupported screen"
  fallback. **Version gate** (2026-09-18, `src/version_gate.cpp`, table `src/game_versions.h`): before any hook, the
  exe + Engine.dll + Game.dll PE timestamps must match a table row, else ONE spoken line ("unsupported game build, exe
  timestamp <hex>, supported ...") and nothing is installed (`GRIMDARK_ANY_VERSION=1` skips it for measuring a patch;
  `tools/archive_build.py` prints the row for a new build). `exe_ui::available()` remains the byte-signature backstop.
  No screen clicks at drawn text any more; the two remaining synthesized clicks land on a widget's OWN rectangle
  (an edit box taking focus, a conversation row) because the game's handler for them has side effects we
  must not bypass.
- **No player-facing "focus mode".** Who gets the keyboard is decided every frame by the screen stack:
  a modelled screen declares `owns_keyboard()` (default true; the game then sees no key events and we drive
  it by clicks/calls), the `unsupported` fallback and any screen where the game itself must handle keys
  (in-game movement, typing into a game text field) return false and every key passes through. The player
  never toggles anything; `/gamekeys` is a dev override that holds until the next screen change.
- **We own the UI state.** Which screen is open, what is focused, what the choices are: ours. We drive the
  game to match and read it back only at defined checkpoints. The game's drawn UI is not a state store;
  nothing user-facing derives from screen-space round trips.
- **MessageBuilder everywhere.** Anything spoken with more than one part is composed through
  `gd::core::MessageBuilder` (fragments space-join, list items comma-join, single use). Never hand-build
  ", " in a speech path; never add a second joining helper. All mod-authored wording lives in
  `src/core/strings.h`; composed shapes are `push_*` helpers there. Game text passes through verbatim.
- **English only** (decided). No localization layer.
- **Hints call Enter "click"** ("click to assign an attribute point"): that is the player's word for activating a row. Other keys are named ("Backspace to reclaim a skill point").
- **Speech never interrupts by default**; interrupt only on focus moves and synchronous state feedback.
- **Never hold a `GraphNode*` across frames.** The graph is rebuilt immediate-mode; a stored pointer dangles
  after the next render (the navigator's `last_spoken_node_` crashed the game in the announcer's path walk
  on a Tab landing, 2026-08-21). Remember a `ControlId` and resolve it in the current render when needed
  (`GraphNavigator::last_spoken_node()`). **The same for game entity pointers**: `world::tick` re-resolved the
  locked review target only every 30 frames and projected the cached pointer in between, so picking the locked
  item up fed `WorldCamera::Project` a freed object -- the rare "not responding after a pickup" crash (stacks
  2026-08-30). Hold the object id; `find_entity(id)` every frame you use it.
- `src/core` is engine-free (no Windows, no game types) and unit-tested with doctest
  (`cmake --build build/ninja --target gdcore_tests && build/ninja/gdcore_tests.exe`). Re-run the CMake
  configure after adding files (globs).
- Playability milestone: character creation -> main game with WASD movement, wall tones, and a real
  understanding of how the game targets.

## Conventions
- **Every player-facing KEY goes in README.md** (its Controls tables) in the same change that adds or rebinds it,
  alongside `docs/controls.md`. The README is the player's document: keys and what they do, not every sound or
  internal detail. (Pets and the vendor's Ctrl+Enter both shipped without a README line, 2026-08-26.)
- **CHANGES.md is the player-facing changelog**: every player-visible change (feature, fix, key, option) gets a line under
  `# Unreleased` at the top in the same change -- create that heading if the top section is a released version. Match the
  existing style: plain player language, one `- ` bullet per change, `Fix: ...` / `Untested fix: ...` prefixes (the latter only when the user asks for it), keys in
  backticks as spoken (`alt comma`, `ctrl t`), sub-bullets for caveats; no internals. A release renames the heading to
  `# <version> (<date>)`.
- **Commit each finished change** on main without asking (the user tests in batches and reports bugs back); an
  untested change is still committed, and the report says it is untested.
- Hook by exported name, never by signature scan. Member functions: `this` first; class-by-value returns
  (`std::basic_string`) use a hidden return pointer as the 2nd argument. The game uses
  `basic_string<unsigned short>` (not char16_t) — see `src/msvc_string.h`.
- All engine calls on the game thread (inside `Display::Update` or a hook). Speech may be called from anywhere.
- Localize every string the mod speaks (rule inherited from wotr-access; not yet wired up).
- The user is blind: no focus stealing, no synthesized input to the real desktop.
- **Who owns the game.** Only one party drives the game at a time, and both must know which. Claude launching
  and driving the game through the dev loop (`gd.py launch`: unfocused, muted, dev-server input only) is what
  the scripts are for and is expected -- but ask first unless the user has made approval clear for this stretch
  of work (an explicit "go ahead", or an ongoing task the user handed over). If the user is at the keyboard in
  the game, do not launch, kill, inject or send input; if Claude owns it, the user will say before taking over.
