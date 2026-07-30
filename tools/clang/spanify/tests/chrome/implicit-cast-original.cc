// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

short get_offset() {
  return 5;
}

void fct() {
  auto buf = std::vector<int>(10, 1);
  // Expected rewrite:
  // base::span<int> a = buf;
  int* a = buf.data();
  a[0] = 1;

  short b = 2;
  // Expected rewrite:
  // a = a.subspan(base::checked_cast<size_t>(b + get_offset()));
  a += b + get_offset();
}
