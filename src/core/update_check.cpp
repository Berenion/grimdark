#include "core/update_check.h"
#include <algorithm>
#include <array>
#include <cctype>

namespace gd::core::update {
namespace {
bool is_hex(std::string_view s) {
  if (s.empty()) return false;
  for (char c : s) if (!std::isxdigit((unsigned char)c)) return false;
  return true;
}
bool is_digits(std::string_view s) {
  if (s.empty()) return false;
  for (char c : s) if (!std::isdigit((unsigned char)c)) return false;
  return true;
}

struct Semver {
  std::array<unsigned long, 3> core{};
  std::string_view pre;   // "pad.2" of "v0.6.0-pad.2+build"; "" for a plain release
};
// "v1.2.3[-pre][+build]" -> {1, 2, 3} + pre; false when it is not one.
bool parse_semver(std::string_view t, Semver& out) {
  if (t.empty() || (t[0] != 'v' && t[0] != 'V')) return false;
  t.remove_prefix(1);
  for (int i = 0; i < 3; ++i) {
    if (t.empty() || !std::isdigit((unsigned char)t[0])) return false;
    unsigned long n = 0;
    while (!t.empty() && std::isdigit((unsigned char)t[0])) { n = n * 10 + (unsigned long)(t[0] - '0'); t.remove_prefix(1); }
    out.core[(size_t)i] = n;
    if (i < 2) { if (t.empty() || t[0] != '.') return false; t.remove_prefix(1); }
  }
  if (!t.empty() && t[0] != '-' && t[0] != '+') return false;
  out.pre = {};
  if (!t.empty() && t[0] == '-') out.pre = t.substr(1, t.find('+') - 1);   // npos - 1 is still "to the end"
  return true;
}
// Semver precedence: the core numbers, then a pre-release sorts below the plain release, then the dot-separated
// identifiers one by one (numbers numerically and below words, words as text), a shorter list first.
// So v0.6.0-pad.1 < v0.6.0-pad.2 < v0.6.0-pad.10 < v0.7.0-pad.1 (the fork's tags).
int compare(const Semver& a, const Semver& b) {
  if (a.core != b.core) return a.core < b.core ? -1 : 1;
  if (a.pre.empty() || b.pre.empty()) return a.pre.empty() == b.pre.empty() ? 0 : (a.pre.empty() ? 1 : -1);
  std::string_view x = a.pre, y = b.pre;
  while (!x.empty() && !y.empty()) {
    std::string_view xi = x.substr(0, x.find('.')), yi = y.substr(0, y.find('.'));
    x.remove_prefix(std::min(x.size(), xi.size() + 1));
    y.remove_prefix(std::min(y.size(), yi.size() + 1));
    bool xn = is_digits(xi), yn = is_digits(yi);
    if (xn && yn) {
      if (xi.size() != yi.size()) return xi.size() < yi.size() ? -1 : 1;   // no leading zeros in semver
      if (xi != yi) return xi < yi ? -1 : 1;
    } else if (xn != yn) {
      return xn ? -1 : 1;
    } else if (xi != yi) {
      return xi < yi ? -1 : 1;
    }
  }
  return x.empty() == y.empty() ? 0 : (x.empty() ? -1 : 1);
}
}  // namespace

Channel channel_of(std::string_view installed) {
  Semver v;
  if (parse_semver(installed, v)) return Channel::Release;
  if (installed.size() > 3 && installed.substr(0, 3) == "ci-" && is_hex(installed.substr(3))) return Channel::Ci;
  return Channel::None;
}

std::string request_path(Channel c, std::string_view repo) {
  std::string base = "/repos/" + std::string(repo) + "/releases";
  switch (c) {
    case Channel::Release: return base + "/latest";
    case Channel::Ci: return base + "/tags/ci-latest";
    default: return {};
  }
}

std::string json_string(std::string_view json, std::string_view key) {
  std::string needle = "\"" + std::string(key) + "\"";
  size_t at = json.find(needle);
  if (at == std::string_view::npos) return {};
  size_t i = at + needle.size();
  auto skip_ws = [&] { while (i < json.size() && std::isspace((unsigned char)json[i])) ++i; };
  skip_ws();
  if (i >= json.size() || json[i] != ':') return {};
  ++i; skip_ws();
  if (i >= json.size() || json[i] != '"') return {};
  ++i;
  std::string out;
  for (; i < json.size(); ++i) {
    char c = json[i];
    if (c == '"') return out;
    if (c == '\\' && i + 1 < json.size()) { out += json[++i]; continue; }   // an escaped quote or backslash; \u escapes never appear in a tag or sha
    out += c;
  }
  return {};   // unterminated
}

Verdict decide(std::string_view installed, std::string_view json) {
  Verdict v;
  switch (channel_of(installed)) {
    case Channel::Release: {
      std::string tag = json_string(json, "tag_name");
      Semver have, latest;
      if (parse_semver(installed, have) && parse_semver(tag, latest) && compare(latest, have) > 0) { v.newer = true; v.latest = tag; }
      break;
    }
    case Channel::Ci: {
      std::string sha = json_string(json, "target_commitish");
      std::string_view mine = installed.substr(3);
      if (sha.size() >= mine.size() && is_hex(sha) && sha.compare(0, mine.size(), mine) != 0) { v.newer = true; v.latest = sha.substr(0, 7); }
      break;
    }
    default: break;
  }
  return v;
}

}  // namespace gd::core::update
