#include "screens/pad_controls.h"
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "core/graph_builder.h"
#include "core/screen.h"
#include "core/strings.h"
#include "pad_controls_text.h"
#include "screens/window_base.h"

namespace gd::screens {
using namespace gd::core;
namespace {
bool g_open = false;

struct Section { std::string title; std::vector<std::string> lines; };

// A heading is a line with letters and no lower-case letter; the first line is the file's own title (the screen has
// its name) and blank lines only separate.
bool is_heading(std::string_view s) {
  bool letter = false;
  for (char c : s) {
    if (c >= 'a' && c <= 'z') return false;
    if (c >= 'A' && c <= 'Z') letter = true;
  }
  return letter;
}

const std::vector<Section>& sections() {
  static const std::vector<Section> parsed = [] {
    std::vector<Section> out;
    std::string_view text = kPadControlsText;
    bool first = true;
    while (!text.empty()) {
      size_t nl = text.find('\n');
      std::string_view line = text.substr(0, nl);
      text = nl == std::string_view::npos ? std::string_view() : text.substr(nl + 1);
      while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.remove_suffix(1);
      if (line.empty()) continue;
      if (first) { first = false; continue; }
      if (is_heading(line)) out.push_back({std::string(line), {}});
      else if (!out.empty()) out.back().lines.emplace_back(line);
    }
    return out;
  }();
  return parsed;
}

class PadControlsScreen : public Screen {
 public:
  std::string_view key() const override { return "pad_controls"; }
  bool is_active() override { return g_open; }
  std::string screen_name() const override { return std::string(strings::kPadControls); }
  int layer() const override { return 31; }   // above the list picker (30), like the sound glossary: opens over anything
  bool exclusive() const override { return true; }
  std::vector<InputCategory> input_categories() const override { return {InputCategory::UI}; }
  std::vector<ScreenAction> actions() override { return {{std::string(action_ids::Back), [] { g_open = false; }}}; }
  void on_pop() override { g_open = false; }

  void build(GraphBuilder& b) override {
    b.begin_stop("page");
    int si = 0;
    for (const Section& s : sections()) {
      b.begin_group(ControlId::structural(std::format("padcontrols.{}", si)), row_item(s.title), std::nullopt, true);
      int li = 0;
      for (const std::string& l : s.lines) b.add_item(ControlId::structural(std::format("padcontrols.{}.{}", si, li++)), line_item(l));
      b.end_group();
      ++si;
    }
  }
};
}  // namespace

void open_pad_controls() { g_open = true; }
std::unique_ptr<Screen> make_pad_controls() { return std::make_unique<PadControlsScreen>(); }
}  // namespace gd::screens
