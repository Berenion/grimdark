#include <doctest/doctest.h>
#include "core/update_check.h"

using namespace gd::core::update;

TEST_CASE("update: channel of the embedded version") {
  CHECK(channel_of("v0.4.0") == Channel::Release);
  CHECK(channel_of("v1.10.2-rc1") == Channel::Release);
  CHECK(channel_of("ci-03c9462") == Channel::Ci);
  CHECK(channel_of("dev") == Channel::None);
  CHECK(channel_of("ci-") == Channel::None);
  CHECK(channel_of("ci-xyz") == Channel::None);
  CHECK(channel_of("0.4.0") == Channel::None);
  CHECK(channel_of("v0.6.0-pad.1") == Channel::Release);
  CHECK(request_path(Channel::Release, "ahicks92/grimdark") == "/repos/ahicks92/grimdark/releases/latest");
  CHECK(request_path(Channel::Ci, "Berenion/grimdark") == "/repos/Berenion/grimdark/releases/tags/ci-latest");
  CHECK(request_path(Channel::None, "ahicks92/grimdark").empty());
}

TEST_CASE("update: json string fields") {
  const char* release = "{\"url\":\"x\",\"tag_name\": \"v0.5.0\",\"name\":\"a \\\"q\\\"\"}";
  const char* escaped = "{\"name\":\"a \\\"q\\\" \\\\ b\"}";
  CHECK(json_string(release, "tag_name") == "v0.5.0");
  CHECK(json_string(escaped, "name") == "a \"q\" \\ b");
  CHECK(json_string(R"({"tag_name":5})", "tag_name").empty());
  CHECK(json_string(R"({"other":"v"})", "tag_name").empty());
  CHECK(json_string(R"({"tag_name":"unterminated)", "tag_name").empty());
}

TEST_CASE("update: releases compare by semver") {
  CHECK(decide("v0.4.0", R"({"tag_name":"v0.5.0"})").newer);
  CHECK(decide("v0.4.0", R"({"tag_name":"v0.5.0"})").latest == "v0.5.0");
  CHECK(decide("v0.4.9", R"({"tag_name":"v0.10.0"})").newer);   // numeric, not text
  CHECK_FALSE(decide("v0.5.0", R"({"tag_name":"v0.5.0"})").newer);
  CHECK_FALSE(decide("v0.6.0", R"({"tag_name":"v0.5.0"})").newer);
  CHECK_FALSE(decide("v0.4.0", R"({"message":"Not Found"})").newer);
}

TEST_CASE("update: fork tags compare by pre-release precedence") {
  CHECK(decide("v0.6.0-pad.1", R"({"tag_name":"v0.6.0-pad.2"})").newer);
  CHECK(decide("v0.6.0-pad.2", R"({"tag_name":"v0.6.0-pad.10"})").newer);   // numeric, not text
  CHECK(decide("v0.6.0-pad.9", R"({"tag_name":"v0.7.0-pad.1"})").newer);
  CHECK(decide("v0.6.0-pad", R"({"tag_name":"v0.6.0-pad.1"})").newer);       // a longer list wins a tie
  CHECK(decide("v0.6.0-pad.1", R"({"tag_name":"v0.6.0"})").newer);           // a plain release outranks its pre-release
  CHECK(decide("v0.6.0-1", R"({"tag_name":"v0.6.0-pad"})").newer);           // a number sorts below a word
  CHECK_FALSE(decide("v0.6.0-pad.2", R"({"tag_name":"v0.6.0-pad.2"})").newer);
  CHECK_FALSE(decide("v0.6.0-pad.2", R"({"tag_name":"v0.6.0-pad.1"})").newer);
  CHECK_FALSE(decide("v0.6.0-pad.1+b7", R"({"tag_name":"v0.6.0-pad.1+b8"})").newer);   // build metadata is ignored
  CHECK_FALSE(decide("v0.7.0-pad.1", R"({"tag_name":"v0.6.0-pad.9"})").newer);
  CHECK_FALSE(decide("v0.6.0", R"({"tag_name":"v0.6.0-pad.3"})").newer);
}

TEST_CASE("update: CI builds compare by commit") {
  const char* json = R"({"tag_name":"ci-latest","target_commitish":"26ab37012345678901234567890123456789abcd"})";
  CHECK_FALSE(decide("ci-26ab370", json).newer);
  Verdict v = decide("ci-4d25d72", json);
  CHECK(v.newer);
  CHECK(v.latest == "26ab370");
  CHECK_FALSE(decide("ci-4d25d72", R"({"target_commitish":"main"})").newer);   // not a sha
  CHECK_FALSE(decide("dev", json).newer);
}
