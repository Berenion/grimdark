#include <doctest/doctest.h>
#include "core/gamepad.h"

using namespace gd::core::pad;
using Kind = Command::Kind;

namespace {
struct Pad {
  Translator t;
  State s;
  double now = 0;
  Pad() { s.connected = true; s.family = Family::Xbox; }
  std::vector<Command> frame(Context ctx = Context::World, double dt = 1.0 / 60) { now += dt; return t.update(s, ctx, now); }
  std::vector<Command> press(uint32_t b, Context ctx = Context::World) { s.buttons |= b; return frame(ctx); }
  std::vector<Command> release(uint32_t b, Context ctx = Context::World) { s.buttons &= ~b; return frame(ctx); }
};
bool has_key(const std::vector<Command>& v, int code, bool down) {
  for (auto& c : v) if (c.kind == Kind::Key && c.code == code && c.down == down) return true;
  return false;
}
const Command* find_key(const std::vector<Command>& v, int code, bool down) {
  for (auto& c : v) if (c.kind == Kind::Key && c.code == code && c.down == down) return &c;
  return nullptr;
}
bool has_action(const std::vector<Command>& v, const char* id) {
  for (auto& c : v) if (c.kind == Kind::Action && c.action == id) return true;
  return false;
}
}  // namespace

TEST_CASE("pad: A holds the left mouse key in the world and is Enter in a menu") {
  Pad p;
  CHECK(has_key(p.press(A), key::J, true));
  CHECK(p.frame().empty());   // held: nothing new
  CHECK(has_key(p.release(A), key::J, false));
  CHECK(has_key(p.press(A, Context::Menu), key::Enter, true));
  CHECK(has_key(p.release(A, Context::Menu), key::Enter, false));
}

TEST_CASE("pad: B dodges in the world, also inside a layer, and is Back in a menu") {
  Pad p;
  CHECK(has_key(p.press(B), key::Space, true));
  p.release(B);
  p.press(RT);
  CHECK(has_key(p.press(B), key::Space, true));
  p.release(B | RT);
  CHECK(has_key(p.press(B, Context::Menu), key::Escape, true));
}

TEST_CASE("pad: RT is the quickbar layer and a release always matches its press") {
  Pad p;
  CHECK(p.press(RT).empty());
  CHECK(has_key(p.press(A), key::D1, true));
  p.release(RT);                                   // let go of the layer first
  CHECK(has_key(p.release(A), key::D1, false));    // still the slot key, not J
  p.press(RT);
  CHECK(has_key(p.press(DLeft), key::D1 + 8, true));
  CHECK(has_key(p.press(R3), key::D0, true));
}

TEST_CASE("pad: LT is Shift, and keys pressed under it carry the Shift flag") {
  Pad p;
  auto v = p.press(LT);
  REQUIRE(find_key(v, key::Shift, true));
  v = p.press(A);
  auto* j = find_key(v, key::J, true);
  REQUIRE(j);
  CHECK(j->shift);
  CHECK(has_key(p.release(LT), key::Shift, false));
}

TEST_CASE("pad: an L3 tap is Where am I; L3 held is the mod layer and its release says nothing") {
  Pad p;
  p.press(L3);
  CHECK(has_action(p.release(L3), "ingame.where"));
  p.press(L3);
  CHECK(has_action(p.press(A), "follow.ping"));
  p.release(A);
  CHECK(p.release(L3).empty());
  p.press(L3);
  p.frame(Context::World, 1.0);                    // held a second: not a tap
  CHECK(p.release(L3).empty());
}

TEST_CASE("pad: L3 + right stick flicks are the information commands") {
  Pad p;
  p.press(L3);
  p.s.ry = 1;
  CHECK(has_action(p.frame(), "ingame.objectives"));
  p.s.ry = 0;
  p.frame();
  p.s.ry = -1;
  CHECK(has_action(p.frame(), "rooms.describe"));
  p.s.ry = 0;
  p.frame();
  CHECK(p.release(L3).empty());                    // used: no tap
}

TEST_CASE("pad: the left stick holds W A S D for the game only, diagonals hold two") {
  Pad p;
  p.s.lx = 0.7f; p.s.ly = 0.7f;
  auto v = p.frame();
  auto* w = find_key(v, key::W, true);
  REQUIRE(w);
  CHECK(w->game_only);
  CHECK(has_key(v, key::D, true));
  CHECK(!has_key(v, key::A, true));
  p.s.lx = 0; p.s.ly = 0.9f;                       // straight up: D lets go, W stays
  v = p.frame();
  CHECK(has_key(v, key::D, false));
  CHECK(!has_key(v, key::W, false));
  p.s.ly = 0.1f;                                   // inside the dead zone
  CHECK(has_key(p.frame(), key::W, false));
}

TEST_CASE("pad: opening a menu releases the walking keys") {
  Pad p;
  p.s.ly = 1;
  p.frame();
  auto v = p.frame(Context::Menu);
  CHECK(has_key(v, key::W, false));
  CHECK(has_key(v, key::Up, true));                // the same stick now holds the Up arrow
}

TEST_CASE("pad: right stick flicks drive the scanner once per flick") {
  Pad p;
  p.s.ry = -0.9f;
  auto v = p.frame();
  REQUIRE(v.size() == 1);
  CHECK(v[0].kind == Kind::ScanCategory);
  CHECK(v[0].delta == 1);
  CHECK(p.frame().empty());                        // still pushed: no repeat
  p.s.ry = 0;
  p.frame();
  p.s.rx = -0.9f;
  v = p.frame();
  REQUIRE(v.size() == 1);
  CHECK(v[0].kind == Kind::ScanEntry);
  CHECK(v[0].delta == -1);
}

TEST_CASE("pad: L3 + R3 switches the right stick to the free cursor, which repeats while held") {
  Pad p;
  p.press(L3);
  auto v = p.press(R3);
  REQUIRE(v.size() == 1);
  CHECK(v[0].kind == Kind::FreeCursor);
  CHECK(v[0].on);
  p.release(R3); p.release(L3);
  p.s.ry = 1;
  v = p.frame();
  REQUIRE(v.size() == 1);
  CHECK(v[0].kind == Kind::CursorStep);
  CHECK(v[0].dir == gd::core::CursorKey::Forward);
  CHECK(p.frame(Context::World, 0.1).empty());     // before the repeat delay
  CHECK(p.frame(Context::World, 0.25).size() == 1);
}

TEST_CASE("pad: Start tapped is Escape, held is the pause") {
  Pad p;
  p.press(Start);
  auto v = p.release(Start);
  CHECK(has_key(v, key::Escape, true));
  CHECK(has_key(v, key::Escape, false));
  p.press(Start);
  CHECK(has_action(p.frame(Context::World, 0.6), "game.pause"));
  CHECK(p.release(Start).empty());
}

TEST_CASE("pad: menu shoulders are panels and tabs, the L3 layer adds groups, Home / End and the actions menu") {
  Pad p;
  auto* tab = find_key(p.press(LB, Context::Menu), key::Tab, true);
  REQUIRE(tab);
  CHECK(tab->shift);
  CHECK(!tab->ctrl);
  p.release(LB, Context::Menu);
  tab = find_key(p.press(RT, Context::Menu), key::Tab, true);
  REQUIRE(tab);
  CHECK(tab->ctrl);
  p.release(RT, Context::Menu);
  p.press(L3, Context::Menu);
  CHECK(has_key(p.press(LT, Context::Menu), key::Home, true));
  auto v = p.press(A, Context::Menu);
  REQUIRE(v.size() == 1);
  CHECK(v[0].kind == Kind::ActionsMenu);
}

TEST_CASE("pad: a disconnect lets go of everything") {
  Pad p;
  p.press(A);
  p.s.ly = 1;
  p.frame();
  p.s.connected = false;
  auto v = p.frame();
  CHECK(has_key(v, key::J, false));
  CHECK(has_key(v, key::W, false));
}

TEST_CASE("pad: Sony ids are recognised") {
  SonyModel m;
  CHECK(sony_model(0x054C, 0x0CE6, m));
  CHECK(m == SonyModel::DualSense);
  CHECK(sony_model(0x054C, 0x09CC, m));
  CHECK(m == SonyModel::DualShock4);
  CHECK(!sony_model(0x045E, 0x028E, m));          // an Xbox 360 pad
}

TEST_CASE("pad: a DualShock 4 USB report: sticks, hat, face and shoulder buttons") {
  uint8_t r[64] = {0x01, 255, 0, 128, 128, 0x08 | 0x20, 0x01 | 0x40, 0x02};
  State s;
  REQUIRE(parse_sony_report(r, sizeof r, SonyModel::DualShock4, false, s));
  CHECK(s.family == Family::PlayStation);
  CHECK(s.lx == doctest::Approx(1.0f));
  CHECK(s.ly == doctest::Approx(1.0f));            // byte 0 = pushed up
  CHECK(s.rx == doctest::Approx(0.0f).epsilon(0.01));
  CHECK(s.buttons == (A | LB | L3 | Back));        // hat 8 = released; Cross, L1, L3, touchpad
}

TEST_CASE("pad: a DualSense USB report has its buttons after the triggers and counter") {
  uint8_t r[64] = {0x01, 128, 128, 0, 128, 0, 0, 7, 0x06 | 0x40, 0x20 | 0x80};
  State s;
  REQUIRE(parse_sony_report(r, sizeof r, SonyModel::DualSense, false, s));
  CHECK(s.rx == doctest::Approx(-1.0f).epsilon(0.01));
  CHECK(s.buttons == (DLeft | B | Start | R3));
}

TEST_CASE("pad: DualSense Bluetooth: the short 0x01 report and the full 0x31 one") {
  uint8_t simple[10] = {0x01, 128, 128, 128, 128, 0x00 | 0x80, 0x02, 0};
  State s;
  REQUIRE(parse_sony_report(simple, sizeof simple, SonyModel::DualSense, true, s));
  CHECK(s.buttons == (DUp | Y | RB));
  uint8_t full[78] = {0x31, 0, 128, 255, 128, 128, 0, 0, 0, 0x04 | 0x10, 0x10};
  REQUIRE(parse_sony_report(full, sizeof full, SonyModel::DualSense, true, s));
  CHECK(s.ly == doctest::Approx(-1.0f).epsilon(0.01));
  CHECK(s.buttons == (DDown | X | Back));
  uint8_t other[10] = {0x05};
  CHECK(!parse_sony_report(other, sizeof other, SonyModel::DualSense, true, s));
}

TEST_CASE("pad: merge ORs buttons and keeps the bigger deflection") {
  State a, b;
  a.connected = true; a.family = Family::PlayStation; a.buttons = A; a.lx = 0.2f;
  b.connected = true; b.family = Family::Xbox; b.buttons = B; b.lx = -0.8f;
  State m = merge(a, b);
  CHECK(m.buttons == (A | B));
  CHECK(m.lx == doctest::Approx(-0.8f));
  CHECK(m.family == Family::PlayStation);
}
