#include <windows.h>
#include <string_view>
#include <vector>
#include "app.h"
#include "audio.h"
#include "audio_mute.h"
#include "casts.h"
#include "cues.h"
#include "settings.h"
#include "telegraph.h"
#include "combat.h"
#include "gameapi.h"
#include "notify.h"
#include "quest_rewards.h"
#include "rooms.h"
#include "db.h"
#include "voice.h"
#include "world.h"
#include "version_gate.h"
#include "game_versions.h"
#include "core/strings.h"
#include "exe_ui.h"
#include "devserver.h"
#include "hooks.h"
#include "crash.h"
#include "gamepad.h"
#include "log.h"
#include "speech.h"
#include "update_check.h"

static bool env_flag(const wchar_t* name) {
  wchar_t v[8];
  return GetEnvironmentVariableW(name, v, 8) > 0 && v[0] == L'1';
}
static int env_int(const wchar_t* name, int def) {
  wchar_t v[16];
  return GetEnvironmentVariableW(name, v, 16) > 0 ? _wtoi(v) : def;
}

static bool g_installed = false;   // false = the version gate refused; unload has nothing to tear down

static DWORD WINAPI init_thread(LPVOID) {
  gd::log::init();
  gd::log::write("grimdark: loaded");
  gd::crash::install();   // first: a fault anywhere below leaves its address in the log
  gd::settings::init();
  gd::cues::init();   // the Ctrl+T cue switches / channel volumes (after settings)
  // GRIMDARK_MUTE=1 (set by the dev launcher): speech is recorded but not voiced, game audio session muted.
  bool mute = env_flag(L"GRIMDARK_MUTE");
  gd::speech::set_muted(mute);
  bool sp = gd::speech::init();
  // The version gate: an unknown game build gets one spoken line and no hooks at all (GRIMDARK_ANY_VERSION=1
  // to measure a new patch). Every game-side layer below assumes the build in src/game_versions.h.
  gd::version::Check ver = gd::version::check();
  gd::log::write(gd::version::describe(ver));
  if (!ver.supported && !env_flag(L"GRIMDARK_ANY_VERSION")) {
    std::vector<std::string_view> names;
    for (const auto& b : gd::version::kSupportedBuilds) names.push_back(b.name);
    gd::core::MessageBuilder m;
    gd::speech::speak(gd::strings::push_unsupported_build(m, ver.exe_ts, names).build(), true);
    gd::log::write("grimdark: version gate refused; nothing installed");
    return 0;
  }
  g_installed = true;
  gd::hooks::install();
  gd::world::install();
  gd::gamepad::install();   // the game never sees a pad (src/gamepad.cpp)
  gd::combat::install();
  if (!env_flag(L"GRIMDARK_NOCASTS")) gd::casts::install();
  gd::telegraph::init();   // dev: skip the cast instrumentation
  gd::gameapi::install_crafting_hooks();
  gd::notify::install();
  gd::quest_rewards::install();
  gd::exe_ui::install();
  gd::audio::init();
  gd::voice::init();  // the positional voices (OneCore worker); falls back to the screen reader if it fails
  gd::app::init();
  gd::gamepad::init();
  gd::rooms::init();  // assets/rooms.db (missing = the rooms feature stays silent)
  // The dev server is a player setting (F1 -> mod options, off by default); the dev loop sets GRIMDARK_PORT and
  // gets it regardless, so a hot reload into a dev-launched game keeps its server.
  if (gd::settings::get_bool("devserver", false) || GetEnvironmentVariableW(L"GRIMDARK_PORT", nullptr, 0) > 0)
    gd::dev::start(env_int(L"GRIMDARK_PORT", 8791));
  gd::speech::speak(sp ? "Grimdark loaded" : "Grimdark loaded, no speech backend", true);
  gd::update::start();   // a worker asks GitHub for a newer build; spoken (not interrupting) when there is one
  // Always APPLY the state, both ways: Windows remembers a per-app session mute across launches, so a muted dev
  // run would otherwise leave the next real (speaking) launch silent. Give the game time to open its session.
  Sleep(3000);
  gd::audio::mute_process(mute);
  return 0;
}

// Orderly shutdown, called by the injector on a remote thread BEFORE FreeLibrary. It must not run from
// DllMain: stopping the dev server joins a thread, and a thread cannot exit while DllMain holds the loader
// lock (measured: FreeLibrary from DllMain-side teardown deadlocked the unload thread).
extern "C" __declspec(dllexport) DWORD WINAPI grimdark_unload(LPVOID) {
  gd::log::write("grimdark: unloading");
  if (!g_installed) { gd::crash::remove(); gd::speech::shutdown(); return 1; }   // the gate refused: only these two are up
  gd::dev::stop();
  gd::update::shutdown();
  gd::gamepad::shutdown();
  gd::app::shutdown();
  gd::rooms::shutdown();
  gd::db::shutdown();
  gd::gameapi::remove_crafting_hooks();
  gd::casts::remove();
  gd::combat::remove();
  gd::notify::remove();
  gd::quest_rewards::remove();
  gd::voice::shutdown();  // joins the worker before the mixer it feeds goes away
  gd::audio::shutdown();
  gd::world::remove();
  gd::gamepad::remove();
  gd::hooks::remove();
  gd::crash::remove();
  gd::speech::shutdown();
  // The detours are gone, but the game thread may still be inside one of our hook bodies (the per-frame tick
  // runs from Engine::Update): let any in-flight frame finish before the injector unmaps this DLL.
  Sleep(250);
  gd::log::write("grimdark: hooks removed, ready for FreeLibrary");
  return 1;
}

BOOL WINAPI DllMain(HMODULE h, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(h);
    HANDLE t = CreateThread(nullptr, 0, init_thread, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
  } else if (reason == DLL_PROCESS_DETACH) {
    gd::log::write("grimdark: unloaded");
  }
  return TRUE;
}
