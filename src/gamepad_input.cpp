#include "gamepad_input.h"
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <Xinput.h>
#include <algorithm>
#include <atomic>
#include <cwctype>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "log.h"

namespace gd::padin {
namespace pad = gd::core::pad;

// ======================= XInput =======================
// XInput 1.4 ships with Windows 8+; 9.1.0 is the Windows 7 fallback. Polling an empty slot is slow (it asks the
// driver each time), so a slot that answered "not connected" is only asked again every two seconds.
typedef DWORD(WINAPI* XInputGetState_t)(DWORD, XINPUT_STATE*);
static XInputGetState_t g_xget;
static bool g_xtried;
static bool g_xconnected[XUSER_MAX_COUNT];
static ULONGLONG g_xnext_probe[XUSER_MAX_COUNT];

static float thumb(SHORT v) { float f = v / 32767.0f; return f < -1 ? -1 : f > 1 ? 1 : f; }

static pad::State convert(const XINPUT_GAMEPAD& g) {
  pad::State s;
  s.connected = true;
  s.family = pad::Family::Xbox;
  static constexpr struct { WORD x; pad::Button b; } map[] = {
    {XINPUT_GAMEPAD_A, pad::A}, {XINPUT_GAMEPAD_B, pad::B}, {XINPUT_GAMEPAD_X, pad::X}, {XINPUT_GAMEPAD_Y, pad::Y},
    {XINPUT_GAMEPAD_LEFT_SHOULDER, pad::LB}, {XINPUT_GAMEPAD_RIGHT_SHOULDER, pad::RB},
    {XINPUT_GAMEPAD_LEFT_THUMB, pad::L3}, {XINPUT_GAMEPAD_RIGHT_THUMB, pad::R3},
    {XINPUT_GAMEPAD_START, pad::Start}, {XINPUT_GAMEPAD_BACK, pad::Back},
    {XINPUT_GAMEPAD_DPAD_UP, pad::DUp}, {XINPUT_GAMEPAD_DPAD_DOWN, pad::DDown},
    {XINPUT_GAMEPAD_DPAD_LEFT, pad::DLeft}, {XINPUT_GAMEPAD_DPAD_RIGHT, pad::DRight},
  };
  for (auto& m : map) if (g.wButtons & m.x) s.buttons |= m.b;
  if (g.bLeftTrigger > 64) s.buttons |= pad::LT;
  if (g.bRightTrigger > 64) s.buttons |= pad::RT;
  s.lx = thumb(g.sThumbLX); s.ly = thumb(g.sThumbLY);
  s.rx = thumb(g.sThumbRX); s.ry = thumb(g.sThumbRY);
  return s;
}

static pad::State poll_xinput() {
  if (!g_xtried) {
    g_xtried = true;
    HMODULE m = LoadLibraryW(L"xinput1_4.dll");
    if (!m) m = LoadLibraryW(L"xinput9_1_0.dll");
    if (m) g_xget = (XInputGetState_t)GetProcAddress(m, "XInputGetState");
    log::writef("pad: XInput {}", g_xget ? "ready" : "not available");
  }
  pad::State out;
  if (!g_xget) return out;
  ULONGLONG now = GetTickCount64();
  for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
    if (!g_xconnected[i] && now < g_xnext_probe[i]) continue;
    XINPUT_STATE st{};
    if (g_xget(i, &st) != ERROR_SUCCESS) {
      if (g_xconnected[i]) log::writef("pad: XInput slot {} disconnected", i);
      g_xconnected[i] = false;
      g_xnext_probe[i] = now + 2000;
      continue;
    }
    if (!g_xconnected[i]) log::writef("pad: XInput slot {} connected", i);
    g_xconnected[i] = true;
    out = pad::merge(out, convert(st.Gamepad));
  }
  return out;
}

// ======================= DualShock 4 / DualSense over HID =======================
// Windows gives every open handle its own copy of each input report, so reading never takes the pad from Steam
// or anyone else; a program holding the pad exclusively (DS4Windows' hide mode) makes the open fail, and its
// virtual Xbox pad is then read through XInput instead. A pad plugged in later is found by the next scan.
struct Device {
  std::wstring path;
  pad::SonyModel model;
  bool bluetooth;
  HANDLE handle;
  size_t report_len;
  HANDLE thread = nullptr;
  std::atomic<bool> done{false};
  pad::State state;   // under g_mu
};
static std::mutex g_mu;
static std::vector<std::unique_ptr<Device>> g_devices;
static std::set<std::wstring> g_ignored;          // not a pad we read (checked once per path)
static std::set<std::wstring> g_open_failed;      // logged once; retried quietly on every scan
static HANDLE g_stop;                             // manual-reset: set once on stop()
// Raw thread handles, not std::thread: when the game exits without an unload (the normal case) the threads are
// already gone by DLL_PROCESS_DETACH, and a joinable std::thread destroyed there calls std::terminate.
static HANDLE g_scan;
static void join(HANDLE& t) { if (t) { WaitForSingleObject(t, INFINITE); CloseHandle(t); t = nullptr; } }

static std::string narrow(const std::wstring& w) {
  std::string s;
  for (wchar_t c : w) s.push_back(c < 128 ? (char)c : '?');
  return s;
}
static const char* model_name(pad::SonyModel m) { return m == pad::SonyModel::DualSense ? "DualSense" : "DualShock 4"; }

static DWORD WINAPI read_loop(LPVOID arg) {
  Device* d = (Device*)arg;
  std::vector<uint8_t> buf(d->report_len);
  OVERLAPPED ov{};
  ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  bool stopping = false;
  for (;;) {
    ResetEvent(ov.hEvent);
    DWORD got = 0;
    if (!ReadFile(d->handle, buf.data(), (DWORD)buf.size(), &got, &ov)) {
      if (GetLastError() != ERROR_IO_PENDING) break;
      HANDLE waits[2] = {g_stop, ov.hEvent};
      DWORD w = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
      if (w == WAIT_OBJECT_0) {
        CancelIoEx(d->handle, &ov);
        GetOverlappedResult(d->handle, &ov, &got, TRUE);
        stopping = true;
        break;
      }
      if (!GetOverlappedResult(d->handle, &ov, &got, FALSE)) break;
    }
    pad::State s;
    if (pad::parse_sony_report(buf.data(), got, d->model, d->bluetooth, s)) {
      std::lock_guard lk(g_mu);
      d->state = s;
    }
  }
  CloseHandle(ov.hEvent);
  CloseHandle(d->handle);
  d->handle = INVALID_HANDLE_VALUE;
  if (!stopping) log::writef("pad: {} disconnected", model_name(d->model));
  d->done = true;
  return 0;
}

static bool known(const std::wstring& path) {
  std::lock_guard lk(g_mu);
  for (auto& d : g_devices) if (d->path == path) return true;
  return false;
}

static void try_open(const std::wstring& path) {
  bool sony_path = path.find(L"vid_054c") != std::wstring::npos;
  HANDLE q = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  if (q == INVALID_HANDLE_VALUE) {
    if (sony_path) log::writef("pad: cannot query {} (error {})", narrow(path), GetLastError());
    g_ignored.insert(path);
    return;
  }
  HIDD_ATTRIBUTES a{};
  a.Size = sizeof a;
  pad::SonyModel model{};
  size_t len = 0;
  if (HidD_GetAttributes(q, &a) && pad::sony_model(a.VendorID, a.ProductID, model)) {
    PHIDP_PREPARSED_DATA pp = nullptr;
    if (HidD_GetPreparsedData(q, &pp)) {
      HIDP_CAPS caps{};
      NTSTATUS st = HidP_GetCaps(pp, &caps);
      if (st == HIDP_STATUS_SUCCESS && caps.UsagePage == 0x01 && caps.Usage == 0x05) len = caps.InputReportByteLength;
      else log::writef("pad: Sony collection skipped: status {:#x} usage page {:#x} usage {:#x}", (unsigned)st, caps.UsagePage, caps.Usage);
      HidD_FreePreparsedData(pp);
    } else {
      log::writef("pad: no preparsed data for {} (error {})", narrow(path), GetLastError());
    }
  } else if (sony_path) {
    log::writef("pad: Sony path not recognised: vid {:#06x} pid {:#06x}", a.VendorID, a.ProductID);
  }
  CloseHandle(q);
  if (len == 0) { g_ignored.insert(path); return; }   // another Sony collection (touchpad, audio) or not Sony at all
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    if (g_open_failed.insert(path).second)
      log::writef("pad: cannot read the {} (error {}); another program may hold it exclusively", model_name(model), GetLastError());
    return;
  }
  g_open_failed.erase(path);
  auto d = std::make_unique<Device>();
  d->path = path;
  d->model = model;
  d->bluetooth = path.find(L"00001124-0000-1000-8000-00805f9b34fb") != std::wstring::npos;
  d->handle = h;
  d->report_len = len;
  log::writef("pad: {} over {} connected ({})", model_name(model), d->bluetooth ? "Bluetooth" : "USB", narrow(path));
  Device* raw = d.get();
  std::lock_guard lk(g_mu);
  g_devices.push_back(std::move(d));
  raw->thread = CreateThread(nullptr, 0, read_loop, raw, 0, nullptr);
  if (!raw->thread) raw->done = true;
}

static void scan_once() {
  {   // reap pads whose reads ended (unplugged)
    std::vector<std::unique_ptr<Device>> gone;
    {
      std::lock_guard lk(g_mu);
      for (auto it = g_devices.begin(); it != g_devices.end();)
        if ((*it)->done) { gone.push_back(std::move(*it)); it = g_devices.erase(it); } else ++it;
    }
    for (auto& d : gone) join(d->thread);
  }
  GUID hid;
  HidD_GetHidGuid(&hid);
  HDEVINFO set = SetupDiGetClassDevsW(&hid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (set == INVALID_HANDLE_VALUE) { log::writef("pad: HID enumeration failed (error {})", GetLastError()); return; }
  static bool first = true;
  SP_DEVICE_INTERFACE_DATA data{};
  data.cbSize = sizeof data;
  DWORD i = 0;
  for (; SetupDiEnumDeviceInterfaces(set, nullptr, &hid, i, &data); ++i) {
    DWORD size = 0;
    SetupDiGetDeviceInterfaceDetailW(set, &data, nullptr, 0, &size, nullptr);
    if (size == 0) continue;
    std::vector<BYTE> buf(size);
    auto* detail = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)buf.data();
    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    if (!SetupDiGetDeviceInterfaceDetailW(set, &data, detail, size, nullptr, nullptr)) continue;
    std::wstring path = detail->DevicePath;
    std::transform(path.begin(), path.end(), path.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
    if (g_ignored.count(path) || known(path)) continue;
    try_open(path);
  }
  if (first) { first = false; log::writef("pad: first HID scan saw {} interfaces", i); }
  SetupDiDestroyDeviceInfoList(set);
}

void start() {
  if (g_stop) return;
  g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  g_scan = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
    DWORD wait = 0;
    while (WaitForSingleObject(g_stop, wait) == WAIT_TIMEOUT) {
      scan_once();
      wait = 3000;
    }
    return 0;
  }, nullptr, 0, nullptr);
}

void stop() {
  if (!g_stop) return;
  SetEvent(g_stop);
  join(g_scan);
  std::vector<std::unique_ptr<Device>> all;
  { std::lock_guard lk(g_mu); all.swap(g_devices); }
  for (auto& d : all) join(d->thread);
  CloseHandle(g_stop);
  g_stop = nullptr;
  g_ignored.clear();
  g_open_failed.clear();
}

pad::State poll() {
  pad::State s = poll_xinput();
  std::lock_guard lk(g_mu);
  for (auto& d : g_devices) if (!d->done) s = pad::merge(d->state, s);   // a PlayStation pad names the family
  return s;
}

}  // namespace gd::padin
