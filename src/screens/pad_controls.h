#pragma once
// The controller controls list (quick menu -> "Controller controls"): data/controller_controls.txt, embedded at
// configure time, as one group per section (the file's ALL-CAPS lines) with a read-only row per line.
#include <memory>
namespace gd::core { class Screen; }
namespace gd::screens {
void open_pad_controls();
std::unique_ptr<gd::core::Screen> make_pad_controls();
}
