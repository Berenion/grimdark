// gdlaunch: the player's launcher. Finds the Steam install of Grim Dawn, starts the 64-bit game with grimdark.dll
// loaded before the game initializes (its NVDA-killing keyboard-hook code runs at startup), and keeps this
// console open while the game runs, echoing what the mod says from its log. Every hard failure is a message box
// (a screen reader reads those natively; the mod's speech is not up yet) plus a console line.
//
//   gdlaunch.exe [--game "<path>\x64\Grim Dawn.exe"] [--dry-run]
//   gdlaunch.exe <the game's exe> [game arguments]   (Steam launch options: "<mod folder>\gdlaunch.exe" %command%)
//
// Under Steam's %command% the first plain argument is the exe Steam would have started; the 64-bit exe of that
// install is used whichever one Steam picked, and the arguments after it go on to the game. Steam has then set the
// real launch environment itself, so only what is missing is faked. Run on its own, nothing changes.
//
// Game location, in order: --game; game_path.txt next to this exe (one line); Steam's own records (registry
// SteamPath -> steamapps\libraryfolders.vdf -> the library holding appmanifest_219990.acf -> its installdir);
// the default install path. The mod folder itself can sit anywhere: grimdark.dll, prism.dll and assets\ are
// loaded from next to this exe.
//
// Steam: the game is started the way GDCommunityLauncher does it -- the environment block the Steam client sets
// for a game it launches (SteamAppId etc.) is faked, so the SteamStub wrapper does not relaunch through Steam,
// and steam_api connects to the running client over IPC as usual (achievements, cloud saves). Steam must be
// running for that, which is checked first.
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "../inject/inject_common.h"
#include "core/game_settings.h"

namespace {
const wchar_t* kAppId = L"219990";
const wchar_t* kDefaultExe = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Grim Dawn\\x64\\Grim Dawn.exe";
// A hard failure: console line, message box, exit code 1 (the console closes with the launcher, so the box
// carries the message).
int fail(const wchar_t* title, const std::wstring& text) {
  printf("\n%ls: %ls\n", title, text.c_str());
  fflush(stdout);
  MessageBoxW(nullptr, text.c_str(), title, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
  return 1;
}

bool file_exists(const std::wstring& p) {
  DWORD a = GetFileAttributesW(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring exe_dir() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring s(buf);
  return s.substr(0, s.find_last_of(L"\\/"));
}

std::wstring widen(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  std::wstring out(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
  return out;
}

std::string read_file(const std::wstring& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Every value of `key` in a Valve KeyValues text file ("key"  "value"), with \\ unescaped.
std::vector<std::string> vdf_values(const std::string& text, const char* key) {
  std::vector<std::string> out;
  std::string k = std::string("\"") + key + "\"";
  size_t pos = 0;
  while ((pos = text.find(k, pos)) != std::string::npos) {
    pos += k.size();
    size_t q1 = text.find('"', pos);
    if (q1 == std::string::npos) break;
    size_t q2 = text.find('"', q1 + 1);
    if (q2 == std::string::npos) break;
    std::string v = text.substr(q1 + 1, q2 - q1 - 1), u;
    for (size_t i = 0; i < v.size(); ++i) { if (v[i] == '\\' && i + 1 < v.size() && v[i + 1] == '\\') ++i; u += v[i]; }
    out.push_back(u);
    pos = q2 + 1;
  }
  return out;
}

std::wstring steam_path() {
  wchar_t buf[MAX_PATH]; DWORD n = sizeof buf;
  if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, buf, &n) != ERROR_SUCCESS) return {};
  std::wstring s(buf);
  for (auto& c : s) if (c == L'/') c = L'\\';
  return s;
}

// Steam's own answer to "where is app 219990": the library whose steamapps\ holds its manifest.
std::wstring find_game_via_steam() {
  std::wstring steam = steam_path();
  if (steam.empty()) { printf("Steam is not in the registry (HKCU\\Software\\Valve\\Steam\\SteamPath).\n"); return {}; }
  std::vector<std::wstring> libs{steam};
  for (const std::string& p : vdf_values(read_file(steam + L"\\steamapps\\libraryfolders.vdf"), "path")) libs.push_back(widen(p));
  for (const std::wstring& lib : libs) {
    std::wstring manifest = lib + L"\\steamapps\\appmanifest_" + kAppId + L".acf";
    if (!file_exists(manifest)) continue;
    auto dirs = vdf_values(read_file(manifest), "installdir");
    if (dirs.empty()) continue;
    std::wstring exe = lib + L"\\steamapps\\common\\" + widen(dirs[0]) + L"\\x64\\Grim Dawn.exe";
    printf("Steam library %ls has Grim Dawn (%ls).\n", lib.c_str(), dirs[0].empty() ? L"?" : widen(dirs[0]).c_str());
    if (file_exists(exe)) return exe;
    printf("  but %ls is missing.\n", exe.c_str());
  }
  return {};
}

std::wstring find_game(const std::wstring& arg) {
  if (!arg.empty()) { printf("Game path from the command line.\n"); return arg; }
  std::wstring txt = exe_dir() + L"\\game_path.txt";
  if (file_exists(txt)) {
    std::string line = read_file(txt);
    size_t nl = line.find_first_of("\r\n");
    if (nl != std::string::npos) line.resize(nl);
    if (line.size() >= 3 && (unsigned char)line[0] == 0xEF) line.erase(0, 3);   // UTF-8 BOM from Notepad
    if (!line.empty()) { printf("Game path from game_path.txt.\n"); return widen(line); }
  }
  std::wstring exe = find_game_via_steam();
  if (!exe.empty()) return exe;
  if (file_exists(kDefaultExe)) { printf("Game at the default install path.\n"); return kDefaultExe; }
  return {};
}

bool has_env(const wchar_t* name) {
  wchar_t probe[2];
  return GetEnvironmentVariableW(name, probe, 2) > 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
}

// One argument quoted for a Windows command line (CommandLineToArgvW rules).
std::wstring quote_arg(const std::wstring& a) {
  if (!a.empty() && a.find_first_of(L" \t\"") == std::wstring::npos) return a;
  std::wstring out = L"\"";
  size_t slashes = 0;
  for (wchar_t c : a) {
    if (c == L'\\') { ++slashes; continue; }
    out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
    slashes = 0;
    out += c;
  }
  out.append(slashes * 2, L'\\');
  return out + L"\"";
}

// Whichever Play option Steam picked (x64\Grim Dawn.exe, the 32-bit exe at the install root, compat\Grim Dawn.exe),
// the mod needs the 64-bit exe of that install: the first folder up from the given exe that holds x64\Grim Dawn.exe.
// Empty when none does (find_game then asks Steam's records as usual).
std::wstring x64_exe_near(const std::wstring& exe) {
  std::wstring dir = exe.substr(0, exe.find_last_of(L"\\/"));
  for (int up = 0; up < 4 && !dir.empty(); ++up) {
    std::wstring cand = dir + L"\\x64\\Grim Dawn.exe";
    if (file_exists(cand)) return cand;
    size_t slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) break;
    dir.resize(slash);
  }
  printf("No x64\\Grim Dawn.exe near %ls; looking for the game the usual way.\n", exe.c_str());
  return {};
}

// What the Steam client puts in a game's environment (GDCommunityLauncher's list): the SteamStub wrapper and
// steam_api then treat the process as Steam-launched instead of relaunching it through Steam.
void fake_steam_environment() {
  const wchar_t* vars[][2] = {
    {L"SteamEnv", L"1"}, {L"SteamClientLaunch", L"1"},
    {L"SteamGameId", kAppId}, {L"SteamAppId", kAppId}, {L"SteamOverlayGameId", kAppId},
    {L"SteamAppUser", L"username"}, {L"SteamUser", L"username"}, {L"STEAMID", L"00000000000000000"},
    {L"SESSIONNAME", L"Console"},
  };
  for (auto& v : vars)   // a launch through Steam (%command%) already carries the real values: keep them
    if (!has_env(v[0])) SetEnvironmentVariableW(v[0], v[1]);
  if (has_env(L"USER_MYDOCS")) return;
  PWSTR docs = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs))) { SetEnvironmentVariableW(L"USER_MYDOCS", docs); CoTaskMemFree(docs); }
}

std::wstring log_path() {
  wchar_t base[MAX_PATH];
  DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
  return (n ? std::wstring(base, n) + L"\\Grimdark" : L"C:\\Grimdark") + L"\\grimdark.log";
}

// The game's settings folder: Documents\My Games\Grim Dawn\Settings (the game builds it from the same known folder).
std::wstring settings_dir() {
  PWSTR docs = nullptr;
  std::wstring base;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs))) { base = docs; CoTaskMemFree(docs); }
  if (base.empty()) return {};
  return base + L"\\My Games\\Grim Dawn\\Settings";
}

bool write_file(const std::wstring& p, const std::string& text) {
  HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD n = 0;
  bool ok = WriteFile(h, text.data(), (DWORD)text.size(), &n, nullptr) && n == text.size();
  CloseHandle(h);
  return ok;
}

// Forces the settings the mod cannot play without (core/game_settings.h) in the game's own files before it starts:
// movementType 1 + evadeFollowCursor false in options.txt, and W/S/A/D on the keyboard-mode map's move actions,
// which the game itself leaves unbound. Every other line is kept; the game rewrites both files with the same values
// on exit. A file that cannot be written is reported, never fatal (the game still runs, only movement suffers).
void force_game_settings(bool dry) {
  namespace gs = gd::core::game_settings;
  std::wstring dir = settings_dir();
  if (dir.empty()) { printf("Settings: the Documents folder is unknown; leaving the game's settings alone.\n"); return; }
  if (!dry) {   // the game creates the tree on its first run; a launch through the mod may come first
    for (size_t i = 3; i <= dir.size(); ++i)
      if (i == dir.size() || dir[i] == L'\\') CreateDirectoryW(dir.substr(0, i).c_str(), nullptr);
  }
  struct Job { const wchar_t* file; gs::Patch (*patch)(const std::string&); };
  const Job jobs[] = {
      {L"options.txt", [](const std::string& t) { return gs::patch_options(t, gs::required_options()); }},
      {L"alternate_keybindings.txt", [](const std::string& t) { return gs::patch_keymap(t); }},
  };
  for (const Job& job : jobs) {
    std::wstring path = dir + L"\\" + job.file;
    bool existed = file_exists(path);
    gs::Patch p = job.patch(existed ? read_file(path) : std::string());
    if (!p.changed()) continue;
    for (const auto& c : p.changes) printf("Settings: %ls: %s%s\n", job.file, c.c_str(), dry ? " (dry run: not written)" : "");
    if (dry) continue;
    if (!write_file(path, p.text)) printf("Settings: could not write %ls (error %lu); the game keeps its old values.\n", path.c_str(), GetLastError());
    else if (!existed) printf("Settings: created %ls\n", path.c_str());
  }
  fflush(stdout);
}

// Echo the mod's log lines worth a player's attention as they appear (the DLL truncates the log when it loads).
struct LogTail {
  std::wstring path = log_path();
  size_t offset = 0;
  std::string carry;
  bool saw_loaded = false;   // the DLL got as far as its init thread in this process
  void poll() {
    std::string all = read_file(path);
    if (all.size() < offset) offset = 0;   // truncated (a reload)
    carry += all.substr(offset);
    offset = all.size();
    size_t nl;
    while ((nl = carry.find('\n')) != std::string::npos) {
      std::string line = carry.substr(0, nl);
      carry.erase(0, nl + 1);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.find("grimdark: loaded") != std::string::npos) saw_loaded = true;
      bool show = line.find("[speak") != std::string::npos || line.find("version:") != std::string::npos ||
                  line.find("grimdark:") != std::string::npos || line.find("crash:") != std::string::npos ||
                  line.find("speech:") != std::string::npos;
      if (show) { printf("  %s\n", line.c_str()); fflush(stdout); }
    }
  }
};

}  // namespace

int wmain(int argc, wchar_t** argv) {
  SetConsoleTitleW(L"Grimdark");
  std::wstring game_arg, game_args;
  bool dry = false, from_steam = false;
  for (int i = 1; i < argc; ++i) {
    if (!_wcsicmp(argv[i], L"--game") && i + 1 < argc) game_arg = argv[++i];
    else if (!_wcsicmp(argv[i], L"--dry-run")) dry = true;
    else if (argv[i][0] != L'-' && file_exists(argv[i])) {   // Steam's %command%: the exe, then the game's own arguments
      game_arg = x64_exe_near(argv[i]);
      from_steam = true;
      for (int j = i + 1; j < argc; ++j) game_args += L" " + quote_arg(argv[j]);
      break;
    } else {
      printf("usage: gdlaunch [--game <path to x64\\Grim Dawn.exe>] [--dry-run]\n"
             "   or, in Steam's launch options: \"<mod folder>\\gdlaunch.exe\" %%command%%\n");
      return 2;
    }
  }
  printf("Grimdark launcher\n\n");
  if (from_steam) printf("Started by Steam (launch options with %%command%%).\n");

  std::wstring dir = exe_dir();
  std::wstring dll = dir + L"\\grimdark.dll";
  if (!file_exists(dll)) return fail(L"Grimdark", L"grimdark.dll is not next to gdlaunch.exe. Unzip the whole mod folder and run gdlaunch.exe from inside it.");
  if (!file_exists(dir + L"\\prism.dll")) return fail(L"Grimdark", L"prism.dll is missing from the mod folder. Unzip the whole mod folder again.");

  std::wstring exe = find_game(game_arg);
  if (exe.empty()) return fail(L"Grimdark", L"Grim Dawn was not found. Is it installed through Steam? If it is somewhere unusual, put the full path of x64\\Grim Dawn.exe on the first line of a file named game_path.txt next to gdlaunch.exe.");
  if (!file_exists(exe)) return fail(L"Grimdark", L"The game exe does not exist: " + exe);
  {
    std::wstring d = exe.substr(0, exe.find_last_of(L"\\/"));
    std::wstring leaf = d.substr(d.find_last_of(L"\\/") + 1);
    if (_wcsicmp(leaf.c_str(), L"x64") != 0) return fail(L"Grimdark", L"Grimdark needs the 64-bit game (the exe under the x64 folder), not: " + exe);
  }
  printf("Game: %ls\n", exe.c_str());

  if (!find_pid(L"steam.exe")) return fail(L"Steam is not running", L"Start Steam, sign in, and then run Grimdark again. The game needs Steam running in the background.");
  if (find_pid(L"Grim Dawn.exe")) return fail(L"Grim Dawn is already running", L"Close the running game first (the mod has to be loaded before the game starts), then run Grimdark again.");
  printf("Steam is running.\n");

  force_game_settings(dry);
  if (dry) { printf("Dry run: would start the game with %ls\n", dll.c_str()); return 0; }

  fake_steam_environment();
  {   // start the log fresh so the tail below never echoes a previous run (the DLL truncates it again when it loads)
    HANDLE h = CreateFileW(log_path().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
  }
  std::wstring cwd = install_root_of(exe);
  std::wstring cmd = L"\"" + exe + L"\"" + game_args;
  STARTUPINFOW si{};
  si.cb = sizeof si;
  {   // dev: GRIMDARK_NOFOCUS=1 (the DLL then blocks the game's own focus grabs) also keeps the initial window from activating
    wchar_t v[4];
    if (GetEnvironmentVariableW(L"GRIMDARK_NOFOCUS", v, 4) > 0 && v[0] == L'1') { si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_SHOWNOACTIVATE; }
  }
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED | NORMAL_PRIORITY_CLASS, nullptr, cwd.c_str(), &si, &pi))
    return fail(L"Grimdark", L"Could not start the game (Windows error " + std::to_wstring(GetLastError()) + L"): " + exe);
  printf("Started the game (pid %lu), loading the mod...\n", pi.dwProcessId);
  if (!inject_dll(pi.hProcess, dll.c_str())) {
    TerminateProcess(pi.hProcess, 1);   // a game without the mod would break the screen reader's keys on the first focus change
    return fail(L"Grimdark", L"The mod could not be loaded into the game, so the game was closed again. The log is " + log_path());
  }
  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);
  printf("Mod loaded. The game is starting; it says \"Grimdark loaded\" when the mod is up.\n");
  printf("Leave this window open; it shows what the mod says. Log: %ls\n\n", log_path().c_str());
  fflush(stdout);

  // Follow the game: echo the log, notice a SteamStub relaunch (should not happen with the faked environment,
  // but then the new process needs the DLL too), and report how it ended.
  DWORD pid = pi.dwProcessId;
  HANDLE proc = pi.hProcess;
  LogTail tail;
  DWORD exit_code = 0;
  for (;;) {
    DWORD w = WaitForSingleObject(proc, 500);
    tail.poll();
    if (w != WAIT_OBJECT_0) continue;
    GetExitCodeProcess(proc, &exit_code);
    CloseHandle(proc);
    if (tail.saw_loaded) break;   // a real run ended (the stub relaunches before our DLL's init could log)
    // Gone before the mod initialized and another Grim Dawn.exe comes up = the stub relaunched it; adopt that one.
    DWORD other = 0;
    for (int i = 0; i < 25 && !other; ++i) { other = find_pid(L"Grim Dawn.exe"); if (!other) Sleep(200); }
    if (other && other != pid && !tail.saw_loaded) {
      printf("The game restarted itself (pid %lu); loading the mod there.\n", other);
      proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, other);
      if (!proc) return fail(L"Grimdark", L"The game restarted itself and the mod could not follow it (Windows error " + std::to_wstring(GetLastError()) + L").");
      if (!inject_dll(proc, dll.c_str())) { TerminateProcess(proc, 1); return fail(L"Grimdark", L"The mod could not be loaded into the restarted game, so it was closed again."); }
      pid = other;
      continue;
    }
    break;
  }
  tail.poll();
  if (exit_code == 0) printf("\nThe game exited normally.\n");
  else printf("\nThe game exited with code 0x%lx. If it crashed, the log has the details: %ls\n", exit_code, log_path().c_str());
  return 0;
}
