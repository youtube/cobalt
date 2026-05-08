// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_TIME_DELTA_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_TIME_DELTA_H_

#include <cstdint>

namespace starboard {

// Represents a duration or elapsed amount of media time.
// Internally represented in nanoseconds to provide high precision
// while maintaining a small footprint (8 bytes).
class MediaTimeDelta {
 public:
  constexpr MediaTimeDelta() : time_ns_(0) {}

  static constexpr MediaTimeDelta FromNanoseconds(int64_t nsecs) {
    return MediaTimeDelta(nsecs);
  }
  static constexpr MediaTimeDelta FromMicroseconds(int64_t usecs) {
    return MediaTimeDelta(usecs * 1'000);
  }
  static constexpr MediaTimeDelta FromMilliseconds(int64_t msecs) {
    return MediaTimeDelta(msecs * 1'000'000);
  }
  static constexpr MediaTimeDelta FromSeconds(double secs) {
    return MediaTimeDelta(Round(secs * 1'000'000'000));
  }

  // Getters.
  constexpr int64_t InNanoseconds() const { return time_ns_; }
  constexpr int64_t InMicroseconds() const { return time_ns_ / 1'000; }
  constexpr int64_t InMilliseconds() const { return time_ns_ / 1'000'000; }
  constexpr double InSeconds() const {
    return static_cast<double>(time_ns_) / 1'000'000'000.0;
  }

  // Basic arithmetic operators.
  constexpr MediaTimeDelta operator+(MediaTimeDelta other) const {
    return MediaTimeDelta(time_ns_ + other.time_ns_);
  }
  constexpr MediaTimeDelta operator-(MediaTimeDelta other) const {
    return MediaTimeDelta(time_ns_ - other.time_ns_);
  }
  constexpr MediaTimeDelta& operator+=(MediaTimeDelta other) {
    time_ns_ += other.time_ns_;
    return *this;
  }
  constexpr MediaTimeDelta& operator-=(MediaTimeDelta other) {
    time_ns_ -= other.time_ns_;
    return *this;
  }

  // Unary negation.
  constexpr MediaTimeDelta operator-() const {
    return MediaTimeDelta(-time_ns_);
  }

  // Scalar multiplication and division.
  constexpr MediaTimeDelta operator*(int64_t scalar) const {
    return MediaTimeDelta(time_ns_ * scalar);
  }
  constexpr MediaTimeDelta operator/(int64_t scalar) const {
    return MediaTimeDelta(time_ns_ / scalar);
  }
  constexpr MediaTimeDelta& operator*=(int64_t scalar) {
    time_ns_ *= scalar;
    return *this;
  }
  constexpr MediaTimeDelta& operator/=(int64_t scalar) {
    time_ns_ /= scalar;
    return *this;
  }

  // Ratio between two deltas.
  constexpr double operator/(MediaTimeDelta other) const {
    return static_cast<double>(time_ns_) / other.time_ns_;
  }

  // Comparison operators.
  friend constexpr bool operator==(MediaTimeDelta a, MediaTimeDelta b) {
    return a.time_ns_ == b.time_ns_;
  }
  friend constexpr bool operator!=(MediaTimeDelta a, MediaTimeDelta b) {
    return a.time_ns_ != b.time_ns_;
  }
  friend constexpr bool operator<(MediaTimeDelta a, MediaTimeDelta b) {
    return a.time_ns_ < b.time_ns_;
  }
  friend constexpr bool operator<=(MediaTimeDelta a, MediaTimeDelta b) {
    return a.time_ns_ <= b.time_ns_;
  }
  friend constexpr bool operator>(MediaTimeDelta a, MediaTimeDelta b) {
    return a.time_ns_ > b.time_ns_;
  }
  friend constexpr bool operator>=(MediaTimeDelta a, MediaTimeDelta b) {
    return a.time_ns_ >= b.time_ns_;
  }

 private:
  static constexpr int64_t Round(double d) {
    return static_cast<int64_t>(d >= 0 ? d + 0.5 : d - 0.5);
  }

  // Private constructor to force use of factory methods.
  constexpr explicit MediaTimeDelta(int64_t time_ns) : time_ns_(time_ns) {}

  int64_t time_ns_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_TIME_DELTA_H_
