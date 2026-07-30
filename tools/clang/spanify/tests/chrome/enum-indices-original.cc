// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum UnsignedEnum : unsigned int { kA };
enum SignedEnum : int { kB };

void TestEnums(int* buf, UnsignedEnum u, SignedEnum s) {
  int local_buf[10];
  // Expected rewrite:
  // base::span<int> p1 = base::span<int>(local_buf).subspan(u);
  // (No checked_cast for unsigned enum)
  int* p1 = &local_buf[u];
  p1[0] = 0;

  // Expected rewrite:
  // base::span<int> p2 =
  //     base::span<int>(local_buf).subspan(base::checked_cast<size_t>(s));
  // (Checked_cast for signed enum)
  int* p2 = &local_buf[s];
  p2[0] = 0;
}
