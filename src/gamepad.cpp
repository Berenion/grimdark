#include "gamepad.h"
#include <windows.h>
#include <string>
#include <vector>
#include "app.h"
#include "core/gamepad.h"
#include "core/message_builder.h"
#include "core/strings.h"
#include "gamepad_input.h"
#include "gd_names.h"
#include "hooks.h"
#include "log.h"
#include "screens/list_picker.h"
#include "speech.h"
#include "world.h"

namespace gd::gamepad {
using namespace gd::names;
namespace pad = gd::core::pad;

// ======================= the game is blinded to every pad =======================
// Static RE 2026-10-28 (1.3.1.1): the game reads pads in exactly two places. DirectInput.dll's
// DirectInputDevice::ProcessJoystickEvents asks xinput1_3!XInputGetState (by ordinal 2, slot 0 only) every frame and
// retries every 4 s after "not connected"; Engine.dll's SteamControllerDevice::Update (called only from
// Steamworks::Update) pulls Steam Input. What a pad changes in play is GameEngine's input mode (+0x37748, 0 = keyboard
// and mouse): skill requests, evade, pickup range, Player::FindPath and tooltip text branch on it, and the exe's WASD
// routine only runs at 0. Game.dll never sets it; the exe does, through the exported SetInputMode.
typedef void (*SteamControllerUpdate_t)(void*, int);
static SteamControllerUpdate_t SteamControllerUpdate_hook_orig;
static void SteamControllerUpdate_hook(void*, int) {}   // never connects (device+4 stays 0), never fills its events
typedef bool (*ControllerIsConnected_t)(const void*);
static ControllerIsConnected_t ControllerIsConnected_hook_orig;
static bool ControllerIsConnected_hook(const void*) { return false; }
typedef void (*SetInputMode_t)(void*, int);
static SetInputMode_t SetInputMode_hook_orig;
static void SetInputMode_hook(void* self, int mode) {
  static int logged = 0;
  if (mode != 0 && logged < 5) { ++logged; log::writef("pad: the game asked for input mode {}; kept at 0 (keyboard and mouse)", mode); }
  SetInputMode_hook_orig(self, 0);
}
typedef DWORD(WINAPI* XInputGetState_t)(DWORD, void*);
static XInputGetState_t XInputGetState_hook_orig;
static DWORD WINAPI XInputGetState_hook(DWORD, void*) { return ERROR_DEVICE_NOT_CONNECTED; }

static std::vector<hooks::Hook> g_hooks, g_late;
static bool g_xinput_hooked = false;

bool install() {
  g_hooks = {GD_HOOK(SteamControllerDevice_Update, SteamControllerUpdate_hook),
             GD_HOOK(Steamworks_ControllerIsConnected, ControllerIsConnected_hook),
             GD_HOOK(GameEngine_SetInputMode, SetInputMode_hook)};
  return hooks::attach_hooks(g_hooks) == 0;
}

void remove() {
  hooks::detach_hooks(g_hooks);
  hooks::detach_hooks(g_late);
  g_xinput_hooked = false;
}

// The game's own XInput DLL is a static import of DirectInput.dll, which the game loads after startup.
static void hook_game_xinput() {
  if (g_xinput_hooked || !GetModuleHandleA("xinput1_3.dll")) return;
  g_xinput_hooked = true;
  g_late = {hooks::Hook{"xinput1_3.dll", "XInputGetState", (void*)&XInputGetState_hook, (void**)&XInputGetState_hook_orig,
                        "xinput1_3 XInputGetState", false}};
  hooks::attach_hooks(g_late);
}

// ======================= the mod's own reading =======================
static pad::Translator g_pad;
static bool g_connected = false;
static pad::Family g_family = pad::Family::None;
static std::string g_pending;   // an action picked in one of our menus, run next frame once the picker has closed

void init() { padin::start(); }
void shutdown() { padin::stop(); }

// ---- the right-stick scanner: the finding keys' review groups, one category at a time ----
struct Category { std::string_view name; std::string (*cycle)(int dir, bool nearest); };
static const Category kCategories[] = {
  {strings::kScanEnemies, [](int d, bool n) { return world::cycle_review(world::ScanGroup::Enemies, d, n); }},
  {strings::kScanStrongest, [](int d, bool n) { return world::cycle_highest_classification(d, n); }},
  {strings::kScanPeople, [](int d, bool n) { return world::cycle_review(world::ScanGroup::Neutrals, d, n); }},
  {strings::kScanLoot, [](int d, bool n) { return world::cycle_review(world::ScanGroup::Objects, d, n); }},
  {strings::kScanExits, [](int d, bool n) { return world::cycle_review(world::ScanGroup::Exits, d, n); }},
  {strings::kScanBystanders, [](int d, bool n) { return world::cycle_review(world::ScanGroup::Bystanders, d, n); }},
  {strings::kScanPets, [](int d, bool n) { return world::cycle_review(world::ScanGroup::Pets, d, n); }},
  {strings::kScanCharacters, [](int d, bool n) { return world::cycle_review(world::ScanGroup::Players, d, n); }},
};
static constexpr int kCategoryCount = (int)(sizeof kCategories / sizeof kCategories[0]);
static int g_category = 0;

static void scan_category(int delta) {
  g_category = ((g_category + delta) % kCategoryCount + kCategoryCount) % kCategoryCount;
  const Category& c = kCategories[g_category];
  core::MessageBuilder m;
  m.fragment(c.name).fragment(c.cycle(1, true));
  speech::speak(m.build(), true);
}
static void scan_entry(int delta) { speech::speak(kCategories[g_category].cycle(delta, false), true); }

// ---- our two menus: rows are the mod's registered actions, by their own labels ----
static void open_action_menu(std::string_view title, std::vector<std::string> ids) {
  std::vector<screens::PickerItem> items;
  std::vector<std::string> kept;
  for (auto& id : ids) {
    core::InputAction* a = app::input().find(id);
    if (!a) continue;
    kept.push_back(id);
    items.push_back({(unsigned)kept.size(), a->label(), {}});
  }
  screens::open_picker(std::string(title), std::move(items), [kept](unsigned i) {
    if (i >= 1 && i <= kept.size()) g_pending = kept[i - 1];
  });
}

static void quick_menu() {
  open_action_menu(strings::kPadQuickMenu,
                   {"game.character", "game.skills", "game.map", "game.codex", "game.factions", "game.riftgate", "game.lootFilter",
                    "ingame.pets", "ingame.hotbarManager", "ingame.announcements", "ingame.cueSettings", "cursor.mode", "game.pause",
                    "mod.menu"});
}

static void actions_menu() {
  std::vector<std::string> ids = {"assign.primary", "assign.secondary"};
  for (int k = 1; k <= 10; ++k) ids.push_back("assign.slot" + std::to_string(k));
  ids.push_back("window.ctrlEnter");
  ids.push_back("window.drop");
  open_action_menu(strings::kPadActions, std::move(ids));
}

// The world = the in-game screen has the focus. It owns the keyboard too (it passes the game its frequent keys through
// the key filter), so ownership does not tell a menu from the world; any window or overlay above it is current instead.
static pad::Context context() {
  core::Screen* s = app::screens().current();
  return s && s->key() == "in_game" ? pad::Context::World : pad::Context::Menu;
}

static void run(const pad::Command& c) {
  using K = pad::Command::Kind;
  switch (c.kind) {
    case K::Key:
      hooks::push_key_event({c.code, !c.down, c.shift, c.ctrl, c.alt, 0, !c.game_only});
      break;
    case K::Action:
      log::writef("pad: action {}", c.action);
      if (!app::fire_action(c.action)) log::writef("pad: no action {}", c.action);
      break;
    case K::ScanCategory: scan_category(c.delta); break;
    case K::ScanEntry: scan_entry(c.delta); break;
    case K::CursorStep: world::free_cursor_step(c.dir); break;
    case K::FreeCursor: speech::speak(c.on ? strings::kPadFreeCursor : strings::kPadScanner, true); break;
    case K::QuickMenu: quick_menu(); break;
    case K::ActionsMenu: actions_menu(); break;
  }
}

void tick() {
  hook_game_xinput();
  if (!g_pending.empty() && !screens::picker_open()) {
    std::string id = std::move(g_pending);
    g_pending.clear();
    log::writef("pad: menu action {}", id);
    app::fire_action(id);
  }
  // A pad is only read while the game has the real focus: nothing reaches the game from a pad the player is using
  // for something else, and everything held is let go when the focus leaves.
  pad::State real = padin::poll();
  if (real.connected != g_connected || (real.connected && real.family != g_family)) {
    core::MessageBuilder m;
    if (real.connected) m.fragment(strings::kPadConnected).fragment(real.family == pad::Family::PlayStation ? strings::kPadPlayStation : strings::kPadXbox);
    else m.fragment(strings::kPadDisconnected);
    std::string line = m.build();
    speech::speak(line, false);
    log::writef("pad: {}", line);
    g_connected = real.connected;
    g_family = real.family;
  }
  pad::State s = hooks::game_has_focus() ? real : pad::State{};
  for (const pad::Command& c : g_pad.update(s, context(), app::now())) run(c);
}

}  // namespace gd::gamepad
