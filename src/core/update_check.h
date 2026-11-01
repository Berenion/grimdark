#pragma once
// The update check's decisions (engine-free; the WinHTTP fetch is src/update_check.cpp). The mod's own version is
// embedded at build time (grimdark_version.h): a tag "v0.4.0" (a release; a fork tags "v0.6.0-pad.1", compared by semver
// pre-release precedence) or "ci-<short sha>" (a CI build); anything else ("dev") is never checked. A release asks GitHub for the newest full release (releases/latest skips the ci-latest
// pre-release); a CI build asks for the rolling ci-latest pre-release, whose target_commitish is the main commit it was
// built from (the workflow never moves it backwards), so "a different commit" means "newer".
#include <string>
#include <string_view>

namespace gd::core::update {

enum class Channel { None, Release, Ci };
Channel channel_of(std::string_view installed);
// The GitHub API path for the channel in the repo "owner/name" ("" for None).
std::string request_path(Channel c, std::string_view repo);
// The first "key": "value" string in a JSON text, unescaped; "" when absent. Enough for the two top-level fields read
// here (tag_name, target_commitish), which no nested object of a release repeats.
std::string json_string(std::string_view json, std::string_view key);

struct Verdict { bool newer = false; std::string latest; };   // latest: the tag, or the CI build's short sha
Verdict decide(std::string_view installed, std::string_view json);

}  // namespace gd::core::update
