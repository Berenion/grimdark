#include "core/gamepad.h"
#include <cmath>
#include <utility>

namespace gd::core::pad {

static float bigger(float a, float b) { return std::fabs(a) >= std::fabs(b) ? a : b; }

State merge(const State& a, const State& b) {
  State s;
  s.connected = a.connected || b.connected;
  s.family = a.connected ? a.family : b.family;
  s.buttons = a.buttons | b.buttons;
  s.lx = bigger(a.lx, b.lx); s.ly = bigger(a.ly, b.ly);
  s.rx = bigger(a.rx, b.rx); s.ry = bigger(a.ry, b.ry);
  return s;
}

bool sony_model(uint16_t vendor, uint16_t product, SonyModel& out) {
  if (vendor != 0x054C) return false;
  switch (product) {
    case 0x05C4: case 0x09CC: case 0x0BA0: out = SonyModel::DualShock4; return true;   // v1, v2, USB wireless adapter
    case 0x0CE6: case 0x0DF2: out = SonyModel::DualSense; return true;                 // DualSense, DualSense Edge
    default: return false;
  }
}

static float axis(uint8_t v) {
  float f = ((int)v - 128) / 127.0f;
  return f < -1 ? -1 : f > 1 ? 1 : f;
}

bool parse_sony_report(const uint8_t* r, size_t n, SonyModel model, bool bluetooth, State& out) {
  if (n < 1) return false;
  // The first button byte (hat + face buttons) by report id; the sticks sit just before it, except in the
  // DualSense's full reports, where the two trigger bytes and a counter come between.
  int off = -1;
  bool full_ds = false;
  if (model == SonyModel::DualSense) {
    if (r[0] == 0x01) { off = bluetooth ? 5 : 8; full_ds = !bluetooth; }
    else if (r[0] == 0x31) { off = 9; full_ds = true; }
  } else {
    if (r[0] == 0x01) off = 5;
    else if (r[0] == 0x11) off = 7;
  }
  if (off < 0 || (size_t)off + 2 >= n) return false;
  int st = full_ds ? off - 7 : off - 4;
  State s;
  s.connected = true;
  s.family = Family::PlayStation;
  s.lx = axis(r[st]); s.ly = -axis(r[st + 1]); s.rx = axis(r[st + 2]); s.ry = -axis(r[st + 3]);
  uint8_t b0 = r[off], b1 = r[off + 1], b2 = r[off + 2];
  static constexpr uint32_t hat[8] = {DUp, DUp | DRight, DRight, DDown | DRight, DDown, DDown | DLeft, DLeft, DUp | DLeft};
  if ((b0 & 0x0F) < 8) s.buttons |= hat[b0 & 0x0F];
  if (b0 & 0x10) s.buttons |= X;       // Square
  if (b0 & 0x20) s.buttons |= A;       // Cross
  if (b0 & 0x40) s.buttons |= B;       // Circle
  if (b0 & 0x80) s.buttons |= Y;       // Triangle
  if (b1 & 0x01) s.buttons |= LB;
  if (b1 & 0x02) s.buttons |= RB;
  if (b1 & 0x04) s.buttons |= LT;
  if (b1 & 0x08) s.buttons |= RT;
  if (b1 & 0x10) s.buttons |= Back;    // Share / Create
  if (b1 & 0x20) s.buttons |= Start;   // Options
  if (b1 & 0x40) s.buttons |= L3;
  if (b1 & 0x80) s.buttons |= R3;
  if (b2 & 0x02) s.buttons |= Back;    // touchpad click
  out = s;
  return true;
}

// Stick thresholds: engage past the first, let go under the second, so a stick resting on the line does not chatter.
static constexpr float kWalkOn = 0.35f, kWalkOff = 0.25f;     // left stick, walking
static constexpr float kFlickOn = 0.6f, kFlickOff = 0.3f;     // right stick flicks (scanner, mod-layer commands)
static constexpr float kNavOn = 0.5f, kNavOff = 0.3f;         // a stick standing in for an arrow key
static constexpr float kDiagonal = 0.383f;                    // sin 22.5 deg: eight equal walking sectors

// 0 up, 1 down, 2 left, 3 right, by the dominant axis.
static int dominant(float x, float y) {
  if (std::fabs(y) >= std::fabs(x)) return y > 0 ? 0 : 1;
  return x < 0 ? 2 : 3;
}

static int index_of(Button b) {
  for (int i = 0; i < kButtonCount; ++i) if ((1u << i) == (uint32_t)b) return i;
  return 0;
}

void Translator::key(std::vector<Command>& out, int code, bool down, bool shift, bool ctrl, bool game_only) const {
  Command c;
  c.kind = Command::Kind::Key;
  c.code = code; c.down = down; c.shift = shift; c.ctrl = ctrl; c.game_only = game_only;
  out.push_back(std::move(c));
}

void Translator::action(std::vector<Command>& out, std::string id) const {
  Command c;
  c.kind = Command::Kind::Action;
  c.action = std::move(id);
  out.push_back(std::move(c));
}

bool Translator::layer_held(Binding::Type t) const {
  for (const Binding& b : held_) if (b.type == t) return true;
  return false;
}
bool Translator::shift_held() const { return layer_held(Binding::Type::ShiftMod); }

Translator::Binding Translator::bind(Button b, Context ctx) const {
  using T = Binding::Type;
  auto k = [](int code, bool shift = false, bool ctrl = false) { Binding r; r.type = T::Key; r.code = code; r.shift = shift; r.ctrl = ctrl; return r; };
  auto a = [](const char* id) { Binding r; r.type = T::Action; r.action = id; return r; };
  auto t = [](T type) { Binding r; r.type = type; return r; };
  bool mod = layer_held(T::ModLayer);
  if (ctx == Context::Menu) {
    if (mod) {
      switch (b) {
        case Y: return k(key::Space, false, true);   // detailed tooltip
        case LB: return k(key::Up, false, true);     // previous group
        case RB: return k(key::Down, false, true);   // next group
        case LT: return k(key::Home);
        case RT: return k(key::End);
        case A: return t(T::ActionsMenu);
        default: break;
      }
    }
    switch (b) {
      case DUp: return k(key::Up);
      case DDown: return k(key::Down);
      case DLeft: return k(key::Left);
      case DRight: return k(key::Right);
      case A: return k(key::Enter);
      case B: return k(key::Escape);
      case X: return k(key::Backspace);
      case Y: return k(key::Space);
      case LB: return k(key::Tab, true);
      case RB: return k(key::Tab);
      case LT: return k(key::Tab, true, true);
      case RT: return k(key::Tab, false, true);
      case R3: return k(key::Backslash);
      case L3: return t(T::ModLayer);
      case Start: return k(key::Escape);
      default: return {};
    }
  }
  // The world. B is the dodge in every layer.
  if (b == B) return k(key::Space);
  if (mod) {
    switch (b) {
      case A: return a("follow.ping");
      case X: return a("follow.review");
      case Y: return a("scan.inspect");
      case LB: return a("ingame.swapWeapons");
      case RB: return k(key::Y);                 // the game's own quickbar switch
      case LT: return a("sonar.toggle");
      case RT: return a("ingame.showAllItems");
      case R3: return t(T::FreeCursorToggle);
      case Start: return a("mod.menu");
      case Back: return t(T::QuickMenu);
      default: return {};                        // the d-pad sits under the same thumb as L3
    }
  }
  if (layer_held(T::QuickbarLayer)) {
    switch (b) {
      case A: return k(key::D1);
      case X: return k(key::D1 + 1);
      case Y: return k(key::D1 + 2);
      case LB: return k(key::D1 + 3);
      case RB: return k(key::D1 + 4);
      case DUp: return k(key::D1 + 5);
      case DRight: return k(key::D1 + 6);
      case DDown: return k(key::D1 + 7);
      case DLeft: return k(key::D1 + 8);
      case R3: return k(key::D0);
      default: break;                            // LT / L3 / Start / Back keep their base meaning
    }
  }
  switch (b) {
    case A: return k(key::J);
    case X: return k(key::I);
    case Y: return a("scan.enemyNearest");
    case LB: return k(key::R);                   // health potion
    case RB: return k(key::E);                   // energy potion
    case LT: return t(T::ShiftMod);              // the game's Hold Position
    case RT: return t(T::QuickbarLayer);
    case L3: return t(T::ModLayer);
    case R3: return a("scan.ping");
    case DUp: return a("ingame.vitals");
    case DDown: return k(key::U);                // the game's Interact
    case DLeft: return a("ingame.pickup");
    case DRight: return a("ingame.walkTo");
    case Start: return t(T::StartButton);
    case Back: return t(T::QuickMenu);
    default: return {};
  }
}

void Translator::press(int i, Button b, Context ctx, double now, std::vector<Command>& out) {
  using T = Binding::Type;
  if (b != L3 && layer_held(T::ModLayer)) l3_used_ = true;
  Binding bd = bind(b, ctx);
  bool world_shift = ctx == Context::World && shift_held();
  down_at_[i] = now;
  switch (bd.type) {
    case T::Key: key(out, bd.code, true, bd.shift || world_shift, bd.ctrl); break;
    case T::Action: action(out, bd.action); break;
    case T::ShiftMod: key(out, key::Shift, true, true); break;
    case T::ModLayer: l3_used_ = false; break;
    case T::StartButton: start_fired_hold_ = false; break;
    case T::QuickMenu: out.push_back({Command::Kind::QuickMenu}); break;
    case T::ActionsMenu: out.push_back({Command::Kind::ActionsMenu}); break;
    case T::FreeCursorToggle: {
      free_cursor_ = !free_cursor_;
      rs_dir_ = -1;
      Command c; c.kind = Command::Kind::FreeCursor; c.on = free_cursor_;
      out.push_back(std::move(c));
      break;
    }
    default: break;
  }
  held_[i] = std::move(bd);
}

void Translator::release(int i, double now, Context ctx, std::vector<Command>& out) {
  using T = Binding::Type;
  Binding bd = std::move(held_[i]);
  held_[i] = {};
  bool world_shift = ctx == Context::World && shift_held();
  switch (bd.type) {
    case T::Key: key(out, bd.code, false, world_shift); break;
    case T::ShiftMod: key(out, key::Shift, false); break;
    case T::ModLayer:
      if (!l3_used_ && now - down_at_[i] <= t_.tap_max && ctx == Context::World) action(out, "ingame.where");
      break;
    case T::StartButton:
      if (!start_fired_hold_) { key(out, key::Escape, true); key(out, key::Escape, false); }
      break;
    default: break;
  }
}

void Translator::release_sticks(std::vector<Command>& out) {
  static constexpr int wasd[4] = {key::W, key::A, key::S, key::D};
  for (int b = 0; b < 4; ++b) if (wasd_ & (1 << b)) key(out, wasd[b], false, false, false, true);
  wasd_ = 0;
  if (ls_arrow_) { key(out, ls_arrow_, false); ls_arrow_ = 0; }
  if (rs_menu_key_) { key(out, rs_menu_key_, false); rs_menu_key_ = 0; }
  rs_dir_ = -1;
}

void Translator::sticks(const State& s, Context ctx, double now, std::vector<Command>& out) {
  // ---- left stick ----
  float lm = std::hypot(s.lx, s.ly);
  if (ctx == Context::World) {
    static constexpr int wasd[4] = {key::W, key::A, key::S, key::D};
    uint8_t want = 0;
    if (lm >= (wasd_ ? kWalkOff : kWalkOn)) {
      if (s.ly > kDiagonal * lm) want |= 1;
      if (s.lx < -kDiagonal * lm) want |= 2;
      if (s.ly < -kDiagonal * lm) want |= 4;
      if (s.lx > kDiagonal * lm) want |= 8;
    }
    for (int b = 0; b < 4; ++b) {
      bool had = wasd_ & (1 << b), has = want & (1 << b);
      if (had && !has) key(out, wasd[b], false, false, false, true);
    }
    for (int b = 0; b < 4; ++b) {
      bool had = wasd_ & (1 << b), has = want & (1 << b);
      if (has && !had) key(out, wasd[b], true, false, false, true);
    }
    wasd_ = want;
  } else {
    static constexpr int arrows[4] = {key::Up, key::Down, key::Left, key::Right};
    int want = 0;
    if (lm >= (ls_arrow_ ? kNavOff : kNavOn)) want = arrows[dominant(s.lx, s.ly)];
    if (want != ls_arrow_) {
      if (ls_arrow_) key(out, ls_arrow_, false);
      if (want) key(out, want, true);
      ls_arrow_ = want;
    }
  }
  // ---- right stick ----
  float rm = std::hypot(s.rx, s.ry);
  if (ctx == Context::Menu) {
    int want = 0;
    if (rm >= (rs_menu_key_ ? kNavOff : kNavOn) && std::fabs(s.ry) >= std::fabs(s.rx)) want = s.ry > 0 ? key::Up : key::Down;
    if (want != rs_menu_key_) {   // Shift+Up / Shift+Down: the tooltip a line at a time
      if (rs_menu_key_) key(out, rs_menu_key_, false);
      if (want) key(out, want, true, true);
      rs_menu_key_ = want;
    }
    return;
  }
  if (rs_dir_ >= 0 && rm < kFlickOff) { rs_dir_ = -1; return; }
  if (free_cursor_) {
    static constexpr CursorKey dirs[4] = {CursorKey::Forward, CursorKey::Back, CursorKey::Left, CursorKey::Right};
    auto step = [&](int d) { Command c; c.kind = Command::Kind::CursorStep; c.dir = dirs[d]; out.push_back(std::move(c)); };
    if (rs_dir_ < 0 && rm >= kNavOn) {
      rs_dir_ = dominant(s.rx, s.ry);
      step(rs_dir_);
      rs_next_repeat_ = now + t_.cursor_repeat_delay;
    } else if (rs_dir_ >= 0 && now >= rs_next_repeat_) {
      rs_dir_ = dominant(s.rx, s.ry);
      step(rs_dir_);
      rs_next_repeat_ = now + t_.cursor_repeat_interval;
    }
    return;
  }
  if (rs_dir_ >= 0 || rm < kFlickOn) return;
  rs_dir_ = dominant(s.rx, s.ry);
  if (layer_held(Binding::Type::ModLayer)) {
    l3_used_ = true;
    static const char* const info[4] = {"ingame.objectives", "rooms.describe", "ingame.unexplored", nullptr};
    if (info[rs_dir_]) action(out, info[rs_dir_]);
    return;
  }
  Command c;
  c.kind = rs_dir_ < 2 ? Command::Kind::ScanCategory : Command::Kind::ScanEntry;
  c.delta = (rs_dir_ == 0 || rs_dir_ == 2) ? -1 : 1;
  out.push_back(std::move(c));
}

std::vector<Command> Translator::update(const State& in, Context ctx, double now) {
  std::vector<Command> out;
  State s = in;
  if (!s.connected) s = State{};
  if (!have_ctx_) { ctx_ = ctx; have_ctx_ = true; }
  if (ctx != ctx_) {   // what the sticks hold belongs to the old context; buttons keep their press-time meaning
    release_sticks(out);
    ctx_ = ctx;
  }
  uint32_t released = prev_ & ~s.buttons, pressed = s.buttons & ~prev_;
  for (int i = 0; i < kButtonCount; ++i) if (released & (1u << i)) release(i, now, ctx, out);
  for (int i = 0; i < kButtonCount; ++i) if (pressed & (1u << i)) press(i, (Button)(1u << i), ctx, now, out);
  prev_ = s.buttons;
  // Start held: the pause, once.
  int si = index_of(Start);
  if (held_[si].type == Binding::Type::StartButton && !start_fired_hold_ && now - down_at_[si] >= t_.start_hold) {
    start_fired_hold_ = true;
    action(out, "game.pause");
  }
  sticks(s, ctx, now, out);
  return out;
}

}  // namespace gd::core::pad
