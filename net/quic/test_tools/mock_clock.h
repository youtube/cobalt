// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_MOCK_CLOCK_H_
#define NET_QUIC_TEST_TOOLS_MOCK_CLOCK_H_

#include "net/quic/quic_clock.h"

#include "base/compiler_specific.h"
#include "base/time.h"

namespace net {

class MockClock : public QuicClock {
 public:
  MockClock();

  virtual ~MockClock();

  virtual uint64 NowInUsec() OVERRIDE;

  void AdvanceTime(WallTime delta) {
    uint64 delta_us = delta * base::Time::kMicrosecondsPerSecond;
    now_ += delta_us;
  }

 private:
  uint64 now_;
};

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_MOCK_CLOCK_H_
