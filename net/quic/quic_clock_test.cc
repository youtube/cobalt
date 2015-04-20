// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_clock.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

TEST(QuicClockTest, Now) {
  QuicClock clock;

  QuicTime start(base::TimeTicks::Now());
  QuicTime now = clock.Now();
  QuicTime end(base::TimeTicks::Now());

  EXPECT_LE(start, now);
  EXPECT_LE(now, end);
}

}  // namespace test
}  // namespace net
