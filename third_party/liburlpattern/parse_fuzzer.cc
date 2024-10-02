// Copyright 2021 The Chromium Authors
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace liburlpattern {
namespace {
absl::StatusOr<std::string> PassThrough(absl::string_view input) {
  return std::string(input);
}

absl::optional<std::string> ParseAndCanonicalize(absl::string_view s) {
  absl::StatusOr<Pattern> pattern = Parse(s, &PassThrough);
  if (!pattern.ok()) {
    LOG(INFO) << "Parse failed with status: " << pattern.status();
    return absl::nullopt;
  }
  return pattern->GeneratePatternString();
}

std::string FancyHexDump(base::StringPiece label, base::StringPiece data) {
  std::string char_line, hex_line;
  for (char c : data) {
    if (!base::IsAsciiPrintable(c))
      char_line.append(" [?]");
    else
      char_line.append(absl::StrFormat("%4c", c));
    hex_line.append(absl::StrFormat("  %02x", c));
  }
  return base::StrCat({label, "\n", char_line, "\n", hex_line});
}

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOG_INFO); }
};
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  // Make a copy of `data` on the heap to enable ASAN to catch OOB accesses.
  std::string pattern_string(reinterpret_cast<const char*>(data), size);

  absl::optional<std::string> canonical = ParseAndCanonicalize(pattern_string);
  if (!canonical)
    return 0;

  // If `Pattern::GeneratePatternString()` generates canonical strings,
  // recanonicalizing one of its outputs should always be a no-op. To test that
  // property, let's check that `ParseAndCanonicalize()` is idempotent, i.e.
  // that `canonical` is a fixed point of the function.
  absl::optional<std::string> canonical2 = ParseAndCanonicalize(*canonical);
  CHECK(canonical2)
      << "Failed to parse canonical pattern from original input.\n"
      << FancyHexDump("original : ", pattern_string) << "\n"
      << FancyHexDump("canonical: ", *canonical);

  CHECK_EQ(*canonical, *canonical2)
      << "Canonical pattern and its recanonicalization are not equal.\n"
      << FancyHexDump("canonical : ", *canonical) << "\n"
      << FancyHexDump("canonical2: ", *canonical2);
  return 0;
}
}  // namespace liburlpattern
