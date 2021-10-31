// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_COLOR_RGBA_H_
#define COBALT_RENDER_TREE_COLOR_RGBA_H_

#include "base/logging.h"

namespace cobalt {
namespace render_tree {

// Note: this does not handle infinities or nans.
inline float clamp(float input, float min, float max) {
#if defined(STARBOARD)
  if (SB_UNLIKELY(input < min)) return min;
  if (SB_UNLIKELY(input > max)) return max;
#else
  if (input < min) return min;
  if (input > max) return max;
#endif
  return input;
}

// Used to specify a color in the RGB (plus alpha) space.
// This color format is referenced by many render_tree objects in order to
// specify a color.
struct ColorRGBA {
 public:
  ColorRGBA() : r_(0), g_(0), b_(0), a_(0) {}

  ColorRGBA(float red, float green, float blue) {
    CheckRange(red);
    r_ = red;
    CheckRange(green);
    g_ = green;
    CheckRange(blue);
    b_ = blue;
    a_ = 1.0f;
  }

  ColorRGBA(float red, float green, float blue, float alpha) {
    CheckRange(red);
    r_ = red;
    CheckRange(green);
    g_ = green;
    CheckRange(blue);
    b_ = blue;
    CheckRange(alpha);
    a_ = alpha;
  }

  // Decodes the color value from 32-bit integer (4 channels, 8 bits each).
  // Note that alpha channel is mandatory, so opaque colors should be encoded
  // as 0xrrggbbff.
  explicit ColorRGBA(uint32_t rgba) {
    a_ = (rgba & 0xff) / 255.0f;
    rgba >>= 8;
    b_ = (rgba & 0xff) / 255.0f;
    rgba >>= 8;
    g_ = (rgba & 0xff) / 255.0f;
    rgba >>= 8;
    r_ = (rgba & 0xff) / 255.0f;
  }

  void set_r(float value) {
    CheckRange(value);
    r_ = value;
  }
  void set_g(float value) {
    CheckRange(value);
    g_ = value;
  }
  void set_b(float value) {
    CheckRange(value);
    b_ = value;
  }
  void set_a(float value) {
    CheckRange(value);
    a_ = value;
  }

  // These functions clamp the color channel values between 0.0f and 1.0f.
  float r() const { return clamp(r_, 0.0f, 1.0f); }
  float g() const { return clamp(g_, 0.0f, 1.0f); }
  float b() const { return clamp(b_, 0.0f, 1.0f); }
  float a() const { return clamp(a_, 0.0f, 1.0f); }

  uint8_t rgb8_r() const { return static_cast<uint8_t>(r() * 255); }

  uint8_t rgb8_g() const { return static_cast<uint8_t>(g() * 255); }

  uint8_t rgb8_b() const { return static_cast<uint8_t>(b() * 255); }

  uint8_t rgb8_a() const { return static_cast<uint8_t>(a() * 255); }

  ColorRGBA& operator=(const ColorRGBA& other) {
    r_ = other.r_;
    g_ = other.g_;
    b_ = other.b_;
    a_ = other.a_;
    return *this;
  }

  ColorRGBA& operator-=(const ColorRGBA& other) {
    r_ -= other.r_;
    g_ -= other.g_;
    b_ -= other.b_;
    a_ -= other.a_;
    return *this;
  }

  ColorRGBA& operator+=(const ColorRGBA& other) {
    r_ += other.r_;
    g_ += other.g_;
    b_ += other.b_;
    a_ += other.a_;
    return *this;
  }

  ColorRGBA& operator+=(const float f) {
    r_ += f;
    b_ += f;
    g_ += f;
    a_ += f;
    return *this;
  }

  ColorRGBA& operator-=(const float f) {
    r_ -= f;
    b_ -= f;
    g_ -= f;
    a_ -= f;
    return *this;
  }

  ColorRGBA& operator*=(const float f) {
    r_ *= f;
    b_ *= f;
    g_ *= f;
    a_ *= f;
    return *this;
  }

  ColorRGBA& operator/=(const float f) {
    float one_over_f(1.0f / f);
    *this *= one_over_f;
    return *this;
  }

  bool HasAlpha() const { return (a_ < 1.0f); }

 private:
  void CheckRange(float value) const {
    DCHECK_LE(0.0f, value);
    DCHECK_GE(1.0f, value);
  }

  float r_, g_, b_, a_;
};

inline bool operator==(const ColorRGBA& lhs, const ColorRGBA& rhs) {
  return lhs.r() == rhs.r() && lhs.g() == rhs.g() && lhs.b() == rhs.b() &&
         lhs.a() == rhs.a();
}

inline bool operator!=(const ColorRGBA& lhs, const ColorRGBA& rhs) {
  return !(lhs == rhs);
}

inline ColorRGBA operator-(const ColorRGBA& lhs, const ColorRGBA& rhs) {
  ColorRGBA color(lhs);
  color -= rhs;
  return color;
}

inline ColorRGBA operator+(const ColorRGBA& lhs, const ColorRGBA& rhs) {
  ColorRGBA color(lhs);
  color += rhs;
  return color;
}

inline ColorRGBA operator+(const ColorRGBA& lhs, const float rhs) {
  ColorRGBA color(lhs);
  color += rhs;
  return color;
}

inline ColorRGBA operator-(const ColorRGBA& lhs, const float rhs) {
  ColorRGBA color(lhs);
  color -= rhs;
  return color;
}

inline ColorRGBA operator*(const ColorRGBA& lhs, const float rhs) {
  ColorRGBA color(lhs);
  color *= rhs;
  return color;
}

inline ColorRGBA operator/(const ColorRGBA& lhs, const float rhs) {
  ColorRGBA color(lhs);
  color /= rhs;
  return color;
}

// Used by tests.
inline std::ostream& operator<<(std::ostream& stream, const ColorRGBA& color) {
  return stream << "rgba(" << color.r() << ", " << color.g() << ", "
                << color.b() << ", " << color.a() << ")";
}

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_COLOR_RGBA_H_
