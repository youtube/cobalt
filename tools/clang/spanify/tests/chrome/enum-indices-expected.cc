// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdint>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"

enum UnsignedEnum : unsigned int { kA };
enum SignedEnum : int { kB };

void TestEnums(int* buf, UnsignedEnum u, SignedEnum s) {
  std::array<int, 10> local_buf;
  // Expected rewrite:
  // base::span<int> p1 = base::span<int>(local_buf).subspan(u);
  // (No checked_cast for unsigned enum)
  base::span<int> p1 = base::span<int>(local_buf).subspan(u);
  p1[0] = 0;

  // Expected rewrite:
  // base::span<int> p2 =
  //     base::span<int>(local_buf).subspan(base::checked_cast<size_t>(s));
  // (Checked_cast for signed enum)
  base::span<int> p2 =
      base::span<int>(local_buf).subspan(base::checked_cast<size_t>(s));
  p2[0] = 0;
}
