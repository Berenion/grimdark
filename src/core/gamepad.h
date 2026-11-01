#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/cursor_step.h"

// The controller layout (README "Controller"), engine-free and unit-tested. A pad's state goes in once per frame
// with the context (the world, or a menu / window / overlay that owns the keyboard); out come the keyboard keys
// the layout stands for and the mod commands it fires. The host turns keys into the same synthetic key events a
// real keyboard makes, so every screen keeps its own keyboard handling: the controller types on the keyboard.
namespace gd::core::pad {

// One merged pad. Bits are the Xbox names; a PlayStation pad maps Cross = A, Circle = B, Square = X, Triangle = Y,
// L1/R1 = LB/RB, L2/R2 = LT/RT, Create = Back, Options = Start, the touchpad click = Back.
enum Button : uint32_t {
  A = 1u << 0, B = 1u << 1, X = 1u << 2, Y = 1u << 3,
  LB = 1u << 4, RB = 1u << 5, LT = 1u << 6, RT = 1u << 7,
  L3 = 1u << 8, R3 = 1u << 9, Start = 1u << 10, Back = 1u << 11,
  DUp = 1u << 12, DDown = 1u << 13, DLeft = 1u << 14, DRight = 1u << 15,
};
inline constexpr int kButtonCount = 16;

enum class Family { None, Xbox, PlayStation };

struct State {
  bool connected = false;
  Family family = Family::None;
  uint32_t buttons = 0;
  float lx = 0, ly = 0, rx = 0, ry = 0;   // -1..1, y positive = stick pushed up
};
// Two sources of the same physical pad (a PlayStation pad read over HID and mirrored as an Xbox pad by Steam or
// DS4Windows) or two pads at once: buttons OR together, each axis takes the larger deflection.
State merge(const State& a, const State& b);

// DualShock 4 / DualSense over HID (XInput never sees them). Layouts as in Linux hid-playstation and SDL; over
// Bluetooth a pad sends a short 0x01 report until something switches it to the full one (DS4 0x11, DualSense 0x31).
enum class SonyModel { DualShock4, DualSense };
bool sony_model(uint16_t vendor, uint16_t product, SonyModel& out);   // false: not a pad we read
// One input report (byte 0 = report id) -> out (connected, family PlayStation). False for any other report.
bool parse_sony_report(const uint8_t* r, size_t n, SonyModel model, bool bluetooth, State& out);

enum class Context { World, Menu };

// A key in the game's Button enum (DIK scancodes, the extended block for arrows / Home / End).
namespace key {
inline constexpr int Escape = 0x01, D1 = 0x02, D0 = 0x0b, Backspace = 0x0e, Tab = 0x0f, W = 0x11, E = 0x12, R = 0x13,
                     Y = 0x15, U = 0x16, I = 0x17, Enter = 0x1c, A = 0x1e, S = 0x1f, D = 0x20, J = 0x24, Shift = 0x2a,
                     Backslash = 0x2b, Space = 0x39, Home = 0x78, Up = 0x79, Left = 0x7b, Right = 0x7c, End = 0x7d,
                     Down = 0x7e;
}

struct Command {
  enum class Kind {
    Key,            // code + down + modifiers; game_only: straight to the game, never seen by the mod (WASD walking)
    Action,         // fire the mod's registered action `action` (app.cpp ids)
    ScanCategory,   // the right-stick scanner: delta -1 / +1 category, landing on its nearest entry
    ScanEntry,      // previous / next entry in the category
    CursorStep,     // free-cursor mode: move the cursor one step
    FreeCursor,     // free-cursor mode switched: on = the right stick moves the cursor
    QuickMenu,      // View / Create in the world
    ActionsMenu,    // L3 + A in a menu: assign / sell part / drop for the focused thing
  };
  Kind kind = Kind::Key;
  int code = 0;
  bool down = false, shift = false, ctrl = false, alt = false, game_only = false;
  std::string action;
  int delta = 0;
  CursorKey dir = CursorKey::Forward;
  bool on = false;
};

struct Timing {
  double tap_max = 0.35;             // L3 / Start released sooner (and nothing else used) = a tap
  double start_hold = 0.5;           // Start held this long = pause
  double cursor_repeat_delay = 0.3;  // free cursor: first repeat, then every cursor_repeat_interval
  double cursor_repeat_interval = 0.08;
};

class Translator {
 public:
  explicit Translator(Timing t = {}) : t_(t) {}
  // One frame. Keys come back as down/up pairs over frames; a context change releases what the sticks hold.
  std::vector<Command> update(const State& s, Context ctx, double now);
  bool free_cursor() const { return free_cursor_; }

 private:
  // What a button does, decided when it goes down and kept until it comes up, so a release always matches its
  // press even if a layer or the context changed in between.
  struct Binding {
    enum class Type { None, Key, Action, ShiftMod, QuickbarLayer, ModLayer, StartButton, QuickMenu, ActionsMenu, FreeCursorToggle };
    Type type = Type::None;
    int code = 0;
    bool shift = false, ctrl = false;
    std::string action;
  };
  Binding bind(Button b, Context ctx) const;
  void press(int i, Button b, Context ctx, double now, std::vector<Command>& out);
  void release(int i, double now, Context ctx, std::vector<Command>& out);
  void key(std::vector<Command>& out, int code, bool down, bool shift = false, bool ctrl = false, bool game_only = false) const;
  void action(std::vector<Command>& out, std::string id) const;
  void sticks(const State& s, Context ctx, double now, std::vector<Command>& out);
  void release_sticks(std::vector<Command>& out);
  bool shift_held() const;
  bool layer_held(Binding::Type t) const;

  Timing t_;
  uint32_t prev_ = 0;
  Context ctx_ = Context::World;
  bool have_ctx_ = false;
  Binding held_[kButtonCount];
  double down_at_[kButtonCount] = {};
  bool l3_used_ = false;            // something was pressed while L3 was down: its release is not a tap
  bool start_fired_hold_ = false;
  bool free_cursor_ = false;
  // left stick: world = the WASD keys it holds (game only), menu = the arrow it holds
  uint8_t wasd_ = 0;                // bit 0 W, 1 A, 2 S, 3 D
  int ls_arrow_ = 0;                // menu: key code held, 0 = none
  // right stick
  int rs_dir_ = -1;                 // engaged direction 0 up 1 down 2 left 3 right, -1 = neutral
  int rs_menu_key_ = 0;             // menu: Shift+Up / Shift+Down held
  double rs_next_repeat_ = 0;
};

}  // namespace gd::core::pad
