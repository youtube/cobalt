// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_chromium_clock.h"

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/time/time.h"

namespace quic {

namespace {
QuicTime approximate_now_{QuicTime::Zero()};
int approximate_now_usage_counter_{0};
}  // namespace

QuicChromiumClock* QuicChromiumClock::GetInstance() {
  static base::NoDestructor<QuicChromiumClock> instance;
  return instance.get();
}

QuicChromiumClock::QuicChromiumClock() = default;

QuicChromiumClock::~QuicChromiumClock() = default;

void QuicChromiumClock::ZeroApproximateNow() {
  approximate_now_ = QuicTime::Zero();
  approximate_now_usage_counter_ = 0;
};

QuicTime QuicChromiumClock::ApproximateNow() const {
  // At the moment, Chrome does not have a distinct notion of ApproximateNow().
  // We should consider implementing this using MessageLoop::recent_time_.
  if (approximate_now_.IsInitialized() &&
      ++approximate_now_usage_counter_ < 16) {
    return approximate_now_;
  }
  return Now();
}

QuicTime QuicChromiumClock::Now() const {
  int64_t ticks = (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
  DCHECK_GE(ticks, 0);
  approximate_now_ = CreateTimeFromMicroseconds(ticks);
  approximate_now_usage_counter_ = 0;
  return approximate_now_;
}

QuicWallTime QuicChromiumClock::WallNow() const {
  const base::TimeDelta time_since_unix_epoch =
      base::Time::Now() - base::Time::UnixEpoch();
  int64_t time_since_unix_epoch_micro = time_since_unix_epoch.InMicroseconds();
  DCHECK_GE(time_since_unix_epoch_micro, 0);
  return QuicWallTime::FromUNIXMicroseconds(time_since_unix_epoch_micro);
}

// static
base::TimeTicks QuicChromiumClock::QuicTimeToTimeTicks(QuicTime quic_time) {
  // QuicChromiumClock defines base::TimeTicks() as equal to
  // quic::QuicTime::Zero(). See QuicChromiumClock::Now() above.
  QuicTime::Delta offset_from_zero = quic_time - QuicTime::Zero();
  int64_t offset_from_zero_us = offset_from_zero.ToMicroseconds();
  return base::TimeTicks() + base::Microseconds(offset_from_zero_us);
}

}  // namespace quic
