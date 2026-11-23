// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/safe_url_pattern_to_value.h"

#include <string_view>
#include <vector>

#include "base/values.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/liburlpattern/options.h"
#include "third_party/liburlpattern/part.h"
#include "third_party/liburlpattern/pattern.h"

namespace web_app {

base::DictValue ToValue(const blink::SafeUrlPattern& pattern) {
  base::DictValue dict;
  auto set_if_not_empty = [&dict](
                              std::string_view field,
                              const std::vector<liburlpattern::Part>& parts) {
    if (parts.empty()) {
      return;
    }
    liburlpattern::Options options = {
        .delimiter_list = "/",
        .prefix_list = "/",
        .sensitive = true,
        .strict = false,
    };
    liburlpattern::Pattern lib_pattern(parts, options,
                                       /*segment_wildcard_regex=*/"[^/]+?");
    dict.Set(field, lib_pattern.GeneratePatternString());
  };

  set_if_not_empty("protocol", pattern.protocol);
  set_if_not_empty("username", pattern.username);
  set_if_not_empty("password", pattern.password);
  set_if_not_empty("hostname", pattern.hostname);
  set_if_not_empty("port", pattern.port);
  set_if_not_empty("pathname", pattern.pathname);
  set_if_not_empty("search", pattern.search);
  set_if_not_empty("hash", pattern.hash);
  return dict;
}

}  // namespace web_app
