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

#ifndef STARBOARD_COMMON_RECT_H_
#define STARBOARD_COMMON_RECT_H_

#include <iosfwd>

#include "starboard/common/size.h"

namespace starboard {

// Defines a 2D rectangle using an origin point (x, y) and a Size (width,
// height).
//
// This struct follows the conventions of Chromium's `gfx::Rect` by grouping the
// origin and dimensions, allowing unified boundary representation.
//
// This is a trivially copyable value type with no ownership constraints and is
// safe to use across any thread.
struct Rect {
  constexpr Rect() : x(0), y(0), size() {}
  constexpr Rect(int x, int y, int width, int height)
      : x(x), y(y), size(width, height) {}
  constexpr Rect(int x, int y, const Size& size) : x(x), y(y), size(size) {}

  constexpr bool IsEmpty() const { return size.width <= 0 || size.height <= 0; }

  constexpr bool operator==(const Rect& other) const {
    return x == other.x && y == other.y && size == other.size;
  }
  constexpr bool operator!=(const Rect& other) const {
    return !(*this == other);
  }

  int x;
  int y;
  Size size;
};

std::ostream& operator<<(std::ostream& os, const Rect& rect);

}  // namespace starboard

#endif  // STARBOARD_COMMON_RECT_H_
