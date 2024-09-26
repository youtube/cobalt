// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>

#include <string>
#include <tuple>
#include <unordered_map>

#include "components/link_header_util/link_header_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace link_header_util {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);
  const auto result = SplitLinkHeader(input);

  for (const auto& pair : result) {
    assert(pair.first < pair.second);
    std::string url;
    std::unordered_map<std::string, absl::optional<std::string>> params;
    std::ignore = ParseLinkHeaderValue(pair.first, pair.second, &url, &params);
  }

  return 0;
}

}  // namespace link_header_util
