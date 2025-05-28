// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

void fct() {
  // Expected rewrite:
  // auto buf = std::to_array<const char*>({
  //     "\\,",
  //     "+++",
  //     "%%%2C",
  //     "@",
  //     "<empty>",
  //     ":::",
  // });
  auto buf = std::to_array<const char*>({
      "\\,",
      "+++",
      "%%%2C",
      "@",
      "<empty>",
      ":::",
  });
  int index = 0;
  buf[index] = nullptr;
}
