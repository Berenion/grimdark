#pragma once
// The physical controllers, read by the mod itself (the game is blinded to them, src/gamepad.cpp): Xbox-style pads
// through XInput 1.4 (never the game's xinput1_3, whose XInputGetState we answer "not connected"), DualShock 4 /
// DualSense through HID on worker threads. Everything present is merged into one pad.
#include "core/gamepad.h"

namespace gd::padin {
void start();                       // the HID scan thread
void stop();                        // cancels reads and joins every thread (unload path, never DllMain)
gd::core::pad::State poll();        // game thread, once per frame
}  // namespace gd::padin
