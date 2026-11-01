#pragma once
// The controller (README "Controller"; layout in core/gamepad.h). The game never sees a pad: its XInput poll is
// told "not connected", its Steam Input device never updates and its input mode stays keyboard and mouse -- the
// mod's whole design rests on the game's keyboard mode (WASD walking is gated on it). The mod reads the pad itself
// (gamepad_input.h) and types the matching keys into the same queue the dev loop's synthetic keys use.
namespace gd::gamepad {
bool install();   // the Engine.dll / Game.dll blinding hooks (xinput1_3 is attached from tick once it is loaded)
void remove();
void init();      // starts the HID reader
void shutdown();  // joins it (unload path)
void tick();      // once per frame from app::tick, game thread
}  // namespace gd::gamepad
