// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_LAYOUT_UNIT_H_
#define COBALT_LAYOUT_LAYOUT_UNIT_H_

#include <cmath>

#include <algorithm>
#include <iostream>
#include <utility>

#include "base/basictypes.h"
#include "base/logging.h"

namespace cobalt {
namespace layout {

// This is used to represent distances and positions during layout.
class LayoutUnit {
 public:
  // The ratio of the LayoutUnit fixed point value to integers.
  static const int kFixedPointRatio = 64;

  LayoutUnit() : value_(0) {
#ifdef _DEBUG
    is_nan_ = false;
#endif
  }
  ~LayoutUnit() {}

  // Constructors.
  explicit LayoutUnit(int value) : value_(value * kFixedPointRatio) {
#ifdef _DEBUG
    int64_t value64 = value * kFixedPointRatio;
    is_nan_ = value_ != value64;
#endif
  }
  explicit LayoutUnit(float value)
      : value_(static_cast<int32_t>(
            floorf(value * static_cast<float>(kFixedPointRatio)))) {
#ifdef _DEBUG
    int64_t value64 = static_cast<int64_t>(
        floorf(value * static_cast<float>(kFixedPointRatio)));
    is_nan_ = value_ != value64;
#endif
  }

  float toFloat() const {
    return static_cast<float>(value_) / kFixedPointRatio;
  }

  void swap(LayoutUnit& value) {
    std::swap(value_, value.value_);
#ifdef _DEBUG
    std::swap(is_nan_, value.is_nan_);
#endif
  }

  // Copy assignment operator.
  LayoutUnit& operator=(LayoutUnit value) {
    value_ = value.value_;
#ifdef _DEBUG
    is_nan_ = is_nan_ || value.is_nan_;
#endif
    return *this;
  }

  bool operator<(LayoutUnit other) const { return value_ < other.value_; }
  bool operator<=(LayoutUnit other) const { return value_ <= other.value_; }
  bool operator>(LayoutUnit other) const { return value_ > other.value_; }
  bool operator>=(LayoutUnit other) const { return value_ >= other.value_; }
  bool operator==(LayoutUnit other) const { return value_ == other.value_; }
  bool operator!=(LayoutUnit other) const { return value_ != other.value_; }

#ifdef _DEBUG
  bool LessThanOrNaN(LayoutUnit other) const {
    return is_nan_ || other.is_nan_ || *this < other;
  }
  bool LessEqualOrNaN(LayoutUnit other) const {
    return is_nan_ || other.is_nan_ || *this <= other;
  }
  bool GreaterThanOrNaN(LayoutUnit other) const {
    return is_nan_ || other.is_nan_ || *this > other;
  }
  bool GreaterEqualOrNaN(LayoutUnit other) const {
    return is_nan_ || other.is_nan_ || *this >= other;
  }
  bool EqualOrNaN(LayoutUnit other) const {
    return is_nan_ || other.is_nan_ || *this == other;
  }
  bool NotEqualOrNaN(LayoutUnit other) const {
    return is_nan_ || other.is_nan_ || *this != other;
  }
  bool IsNaN() const { return is_nan_; }
#else
  bool LessThanOrNaN(LayoutUnit other) const { return *this < other; }
  bool LessEqualOrNaN(LayoutUnit other) const { return *this <= other; }
  bool GreaterThanOrNaN(LayoutUnit other) const { return *this > other; }
  bool GreaterEqualOrNaN(LayoutUnit other) const { return *this >= other; }
  bool EqualOrNaN(LayoutUnit other) const { return *this == other; }
  bool NotEqualOrNaN(LayoutUnit other) const { return *this != other; }
  bool IsNaN() const { return false; }
#endif

  LayoutUnit operator+() const { return LayoutUnit(*this); }
  LayoutUnit operator-() const {
    LayoutUnit res;
    res.value_ = -value_;
    return res;
  }

  LayoutUnit& operator+=(LayoutUnit other) {
#ifdef _DEBUG
    int64_t value64 = value_;
#endif
    value_ += other.value_;
#ifdef _DEBUG
    is_nan_ = is_nan_ || other.is_nan_ ||
              (value_ != (value64 + static_cast<int64_t>(other.value_)));
#endif
    return *this;
  }
  LayoutUnit operator+(LayoutUnit other) const {
    LayoutUnit res(*this);
    return res += other;
  }
  LayoutUnit& operator-=(LayoutUnit other) {
#ifdef _DEBUG
    int64_t value64 = value_;
#endif
    value_ -= other.value_;
#ifdef _DEBUG
    is_nan_ = is_nan_ || other.is_nan_ ||
              (value_ != (value64 - static_cast<int64_t>(other.value_)));
#endif
    return *this;
  }
  LayoutUnit operator-(LayoutUnit other) const {
    LayoutUnit res(*this);
    return res -= other;
  }

  // Scaling math operators.

  // NOLINTNEXTLINE(runtime/references)
  friend LayoutUnit& operator*=(LayoutUnit& a, int b);
  // NOLINTNEXTLINE(runtime/references)
  friend LayoutUnit& operator/=(LayoutUnit& a, int b);
  // NOLINTNEXTLINE(runtime/references)
  friend LayoutUnit& operator*=(LayoutUnit& a, float b);
  // NOLINTNEXTLINE(runtime/references)
  friend LayoutUnit& operator/=(LayoutUnit& a, float b);

 private:
  int32_t value_;
#ifdef _DEBUG
  // When value_ overflows or underflows during construction or during an
  // operation, it's value is undefined. This flags is used to track when that
  // happens. The value propagates to the result of operations where any
  // operand has this flag set, to indicate that there is undefined behavior.
  bool is_nan_;
#endif
};

// Integer scaling math operators.

// NOLINTNEXTLINE(runtime/references)
inline LayoutUnit& operator*=(LayoutUnit& a, int b) {
#ifdef _DEBUG
  int64_t value64 = a.value_;
#endif
  a.value_ *= b;
#ifdef _DEBUG
  a.is_nan_ = a.is_nan_ || (a.value_ != (value64 * b));
#endif
  return a;
}

inline LayoutUnit operator*(LayoutUnit a, int b) { return a *= b; }

inline LayoutUnit operator*(int a, LayoutUnit b) { return b *= a; }

// NOLINTNEXTLINE(runtime/references)
inline LayoutUnit& operator/=(LayoutUnit& a, int b) {
#ifdef _DEBUG
  int64_t value64 = a.value_;
#endif
  a.value_ /= b;
#ifdef _DEBUG
  a.is_nan_ = a.is_nan_ || (a.value_ != (value64 / b));
#endif
  return a;
}

inline LayoutUnit operator/(LayoutUnit a, int b) { return a /= b; }

// Floating point scaling math operators.

// NOLINTNEXTLINE(runtime/references)
inline LayoutUnit& operator*=(LayoutUnit& a, float b) {
#ifdef _DEBUG
  int64_t value64 = a.value_;
#endif
  a.value_ = static_cast<int32_t>(floorf(static_cast<float>(a.value_) * b));
#ifdef _DEBUG
  a.is_nan_ = a.is_nan_ ||
              (a.value_ !=
               static_cast<int64_t>(floorf(static_cast<float>(value64) * b)));
#endif
  return a;
}

inline LayoutUnit operator*(LayoutUnit a, float b) { return a *= b; }

inline LayoutUnit operator*(float a, LayoutUnit b) { return b *= a; }

// NOLINTNEXTLINE(runtime/references)
inline LayoutUnit& operator/=(LayoutUnit& a, float b) {
  a.value_ = static_cast<int32_t>(floorf(static_cast<float>(a.value_) / b));
  return a;
}

inline LayoutUnit operator/(LayoutUnit a, float b) { return a /= b; }

inline std::ostream& operator<<(std::ostream& out, const LayoutUnit& x) {
#ifdef _DEBUG
  if (x.IsNaN()) {
    return out << "NaN(" << x.toFloat() << ")";
  }
#endif
  return out << x.toFloat();
}

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_LAYOUT_UNIT_H_
