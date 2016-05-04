/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_LAYOUT_LAYOUT_UNIT_H_
#define COBALT_LAYOUT_LAYOUT_UNIT_H_

#include <cmath>

#include <algorithm>
#include <iostream>
#include <utility>

#include "base/basictypes.h"
#include "cobalt/base/math.h"

namespace cobalt {
namespace layout {

// This is used to represent distances and positions during layout.
// See ***REMOVED***cobalt-layout-units.
class LayoutUnit {
 public:
  // The ratio of the LayoutUnit fixed point value to integers.
  static const int kFixedPointRatio = 64;

  LayoutUnit() : value_(0) {}
  ~LayoutUnit() {}

  // Constructors.
  explicit LayoutUnit(int value) : value_(value * kFixedPointRatio) {}
  explicit LayoutUnit(float value)
      : value_(static_cast<int32_t>(
            floorf(value * static_cast<float>(kFixedPointRatio)))) {}

  float toFloat() const {
    return static_cast<float>(value_) / kFixedPointRatio;
  }

  void swap(LayoutUnit& value) { std::swap(value_, value.value_); }

  // Copy assignment operator.
  LayoutUnit& operator=(LayoutUnit value) {
    value_ = value.value_;
    return *this;
  }

  bool operator<(LayoutUnit other) const { return value_ < other.value_; }
  bool operator<=(LayoutUnit other) const { return value_ <= other.value_; }
  bool operator>(LayoutUnit other) const { return value_ > other.value_; }
  bool operator>=(LayoutUnit other) const { return value_ >= other.value_; }
  bool operator==(LayoutUnit other) const { return value_ == other.value_; }
  bool operator!=(LayoutUnit other) const { return value_ != other.value_; }

  LayoutUnit operator+() const { return LayoutUnit(*this); }
  LayoutUnit operator-() const {
    LayoutUnit res;
    res.value_ = -value_;
    return res;
  }

  LayoutUnit& operator+=(LayoutUnit other) {
    value_ += other.value_;
    return *this;
  }
  LayoutUnit operator+(LayoutUnit other) const {
    LayoutUnit res(*this);
    return res += other;
  }
  LayoutUnit& operator-=(LayoutUnit other) {
    value_ -= other.value_;
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
};

// Integer scaling math operators.

// NOLINTNEXTLINE(runtime/references)
inline LayoutUnit& operator*=(LayoutUnit& a, int b) {
  a.value_ *= b;
  return a;
}

inline LayoutUnit operator*(LayoutUnit a, int b) { return a *= b; }

inline LayoutUnit operator*(int a, LayoutUnit b) { return b *= a; }

// NOLINTNEXTLINE(runtime/references)
inline LayoutUnit& operator/=(LayoutUnit& a, int b) {
  a.value_ /= b;
  return a;
}

inline LayoutUnit operator/(LayoutUnit a, int b) { return a /= b; }

// Floating point scaling math operators.

// NOLINTNEXTLINE(runtime/references)
inline LayoutUnit& operator*=(LayoutUnit& a, float b) {
  a.value_ = static_cast<int32_t>(floorf(static_cast<float>(a.value_) * b));
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
  return out << x.toFloat();
}

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_LAYOUT_UNIT_H_
