// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/mock_clock.h"

namespace net {

MockClock::MockClock() : now_(0) {
}

MockClock::~MockClock() {
}

uint64 MockClock::NowInUsec() {
  return now_;
}

base::TimeTicks MockClock::Now() {
  base::TimeTicks now;
  return now + base::TimeDelta::FromMicroseconds(now_);
}

}  // namespace net
