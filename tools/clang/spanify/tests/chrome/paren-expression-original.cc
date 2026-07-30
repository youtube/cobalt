// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

void fct() {
  auto buf = std::vector<char>(10, 1);
  // Expected rewrite:
  // base::span<char> expected_data = buf;
  char* expected_data = buf.data();
  expected_data[0] = 1;

  int index = 1;
  // This parenthesized expression should not cause a crash.
  // Expected rewrite:
  // char* temp =
  //     expected_data.subspan(base::checked_cast<size_t>((index))).data();
  char* temp = expected_data + (index);

  // Test multiple nested parentheses
  // Expected rewrite:
  // char* temp2 =
  //     expected_data.subspan(base::checked_cast<size_t>(((index)))).data();
  char* temp2 = expected_data + ((index));

  // Expected rewrite:
  // char* temp3 =
  //     expected_data
  //         .subspan(base::checked_cast<size_t>(((index * 4) / (index * 2))))
  //         .data();
  char* temp3 = expected_data + ((index * 4) / (index * 2));
}
