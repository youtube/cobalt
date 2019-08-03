// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// QuicTime represents one point in time, stored in microsecond resolution.
// This class wrapps the classes DateTimeO and DateTimeOffset.

#ifndef NET_QUIC_QUIC_TIME_H_
#define NET_QUIC_QUIC_TIME_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "net/base/net_export.h"

namespace net {

class NET_EXPORT_PRIVATE QuicTime {
 public:
  // A QuicTime::Delta represents the signed difference between two points in
  // time, stored in microsecond resolution.
  class NET_EXPORT_PRIVATE Delta {
   public:
    // Default constructor initiates to 0.
    Delta();

    explicit Delta(base::TimeDelta delta);

    // Create a object with infinite offset time.
    static Delta Infinite();

    // Converts a number of milliseconds to a time offset.
    static Delta FromMilliseconds(int64 ms);

    // Converts a number of microseconds to a time offset.
    static Delta FromMicroseconds(int64 us);

    // Converts the time offset to a rounded number of milliseconds.
    int64 ToMilliseconds() const;

    // Converts the time offset to a rounded number of microseconds.
    int64 ToMicroseconds() const;

    Delta Add(const Delta& delta) const;

    Delta Subtract(const Delta& delta) const;

    bool IsZero() const;

    bool IsInfinite() const;

   private:
    base::TimeDelta delta_;

    friend class QuicTime;
  };

  // Default constructor initiates to 0.
  QuicTime();

  explicit QuicTime(base::TimeTicks ticks);

  // Create a new QuicTime holding the time_ms.
  static QuicTime FromMilliseconds(int64 time_ms);

  // Create a new QuicTime holding the time_us.
  static QuicTime FromMicroseconds(int64 time_us);

  int64 ToMilliseconds() const;

  int64 ToMicroseconds() const;

  bool IsInitialized() const;

  QuicTime Add(const Delta& delta) const;

  QuicTime Subtract(const Delta& delta) const;

  Delta Subtract(const QuicTime& other) const;

 private:
  base::TimeTicks ticks_;

  friend class QuicClock;
  friend class QuicClockTest;
};

// Non-member relational operators for QuicTime::Delta.
inline bool operator==(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return lhs.ToMicroseconds() == rhs.ToMicroseconds();
}
inline bool operator!=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(lhs == rhs);
}
inline bool operator<(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return lhs.ToMicroseconds() < rhs.ToMicroseconds();
}
inline bool operator>(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return rhs < lhs;
}
inline bool operator<=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(rhs < lhs);
}
inline bool operator>=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(lhs < rhs);
}
// Non-member relational operators for QuicTime.
inline bool operator==(QuicTime lhs, QuicTime rhs) {
  return lhs.ToMicroseconds() == rhs.ToMicroseconds();
}
inline bool operator!=(QuicTime lhs, QuicTime rhs) {
  return !(lhs == rhs);
}
inline bool operator<(QuicTime lhs, QuicTime rhs) {
  return lhs.ToMicroseconds() < rhs.ToMicroseconds();
}
inline bool operator>(QuicTime lhs, QuicTime rhs) {
  return rhs < lhs;
}
inline bool operator<=(QuicTime lhs, QuicTime rhs) {
  return !(rhs < lhs);
}
inline bool operator>=(QuicTime lhs, QuicTime rhs) {
  return !(lhs < rhs);
}

}  // namespace net

#endif  // NET_QUIC_QUIC_TIME_H_
