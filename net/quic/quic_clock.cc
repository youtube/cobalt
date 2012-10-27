// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_clock.h"

#include "base/time.h"

namespace net {

QuicClock::QuicClock() {
}

QuicClock::~QuicClock() {}

uint64 QuicClock::NowInUsec() {
  base::TimeDelta delta = base::Time::Now() - base::Time::UnixEpoch();
  return delta.InMicroseconds();
}

}  // namespace net
