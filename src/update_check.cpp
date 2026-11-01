#include "update_check.h"
#include <windows.h>
#include <winhttp.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include "core/message_builder.h"
#include "core/strings.h"
#include "core/update_check.h"
#include "grimdark_version.h"
#include "log.h"
#include "settings.h"
#include "speech.h"

namespace gd::update {
namespace {
constexpr const char* kSettingKey = "updatecheck";
constexpr size_t kMaxBody = 1 << 20;
std::thread g_worker;
std::mutex g_mu;
HINTERNET g_request = nullptr;   // the request in flight (closing it from shutdown() aborts the blocking calls)
std::atomic<bool> g_cancel{false};
std::string g_pending;               // the line to speak, set by the worker, taken by tick() (under g_mu)
constexpr double kSettleSeconds = 3.0;

std::wstring widen(std::string_view s) { return std::wstring(s.begin(), s.end()); }   // ASCII paths and versions

// GET https://api.github.com<path>; the body, or "" on any failure (logged).
std::string fetch(const std::string& path) {
  std::wstring agent = L"Grimdark/" + widen(kGrimdarkVersion);
  HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) { log::writef("update: WinHttpOpen failed ({})", GetLastError()); return {}; }
  WinHttpSetTimeouts(session, 5000, 5000, 10000, 10000);
  // GRIMDARK_UPDATE_HOST (dev) replaces the host, e.g. "nonexistent.invalid" to exercise the offline path.
  wchar_t host[256] = L"api.github.com";
  DWORD n = GetEnvironmentVariableW(L"GRIMDARK_UPDATE_HOST", host, 256);
  if (n == 0 || n >= 256) wcscpy_s(host, L"api.github.com");
  HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
  HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", widen(path).c_str(), nullptr, WINHTTP_NO_REFERER,
                                                   WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
  std::string body;
  DWORD status = 0;
  if (request) {
    { std::lock_guard<std::mutex> l(g_mu); if (g_cancel) { WinHttpCloseHandle(request); request = nullptr; } else g_request = request; }
  }
  if (request) {
    const wchar_t* headers = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    bool ok = WinHttpSendRequest(request, headers, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr);
    DWORD size = sizeof status;
    if (ok) ok = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    while (ok && !g_cancel) {
      DWORD avail = 0;
      if (!WinHttpQueryDataAvailable(request, &avail) || avail == 0) break;
      size_t at = body.size();
      if (at + avail > kMaxBody) { body.clear(); break; }
      body.resize(at + avail);
      DWORD got = 0;
      if (!WinHttpReadData(request, body.data() + at, avail, &got)) { body.clear(); break; }
      body.resize(at + got);
    }
    if (!ok) log::writef("update: request failed ({})", GetLastError());
    std::lock_guard<std::mutex> l(g_mu);
    if (g_request) { WinHttpCloseHandle(g_request); g_request = nullptr; }   // else shutdown() closed it
  }
  if (connect) WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);
  if (status != 200) { if (status) log::writef("update: GitHub answered {} for {}", status, path); return {}; }
  return body;
}

void run() {
  std::string_view mine = kGrimdarkVersion;
  core::update::Channel ch = core::update::channel_of(mine);
  std::string body = fetch(core::update::request_path(ch, kGrimdarkRepo));
  if (g_cancel || body.empty()) return;
  core::update::Verdict v = core::update::decide(mine, body);
  log::writef("update: installed {}, newer={} latest='{}'", mine, v.newer, v.latest);
  if (!v.newer) return;
  core::MessageBuilder m;
  if (ch == core::update::Channel::Ci) m.list_item().fragment(strings::kNewer).fragment(kGrimdarkAppName).fragment(strings::kCiBuildAvailable).fragment(v.latest);
  else m.list_item().fragment(kGrimdarkAppName).fragment(strings::kUpdateAvailable).fragment(v.latest);
  m.list_item().fragment(strings::kYouHave).fragment(mine);
  m.list_item().fragment(strings::kRunThe).fragment(kGrimdarkAppName).fragment(strings::kInstallerToUpdate);
  std::lock_guard<std::mutex> l(g_mu);
  g_pending = m.build();   // spoken by tick() once the game has settled
}
}  // namespace

// Called every frame on the game thread with the current screen. The startup speech would talk over the line, so it
// waits until the main menu (or the world, after a hot reload) has stayed current for kSettleSeconds. "Any screen but
// loading" was not enough: the title / intro sit on the "unsupported" fallback for seconds before the menu exists.
void tick(std::string_view screen_key, double now) {
  static std::string last_key;
  static double since = 0;
  if (screen_key != last_key) { last_key = std::string(screen_key); since = now; }
  if ((screen_key != "main_menu" && screen_key != "in_game") || now - since < kSettleSeconds) return;
  std::string line;
  { std::lock_guard<std::mutex> l(g_mu); line.swap(g_pending); }
  if (line.empty()) return;
  log::writef("update: announced on {}", screen_key);
  speech::speak(line, false);
}

bool enabled() { return settings::get_bool(kSettingKey, true); }
void set_enabled(bool on) { settings::set_bool(kSettingKey, on); }

void start() {
  log::writef("grimdark: version {}", kGrimdarkVersion);
  if (core::update::channel_of(kGrimdarkVersion) == core::update::Channel::None || !enabled() || g_worker.joinable()) return;
  g_cancel = false;
  g_worker = std::thread(run);
}

void shutdown() {
  g_cancel = true;
  { std::lock_guard<std::mutex> l(g_mu); if (g_request) { WinHttpCloseHandle(g_request); g_request = nullptr; } }
  if (g_worker.joinable()) g_worker.join();
}
}  // namespace gd::update
