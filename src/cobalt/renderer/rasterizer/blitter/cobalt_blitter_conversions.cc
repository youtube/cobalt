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

#include "cobalt/renderer/rasterizer/blitter/cobalt_blitter_conversions.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

namespace {
int RoundToInt(float value) {
  return static_cast<int>(std::floor(value + 0.5f));
}
}  // namespace

math::Rect RectFToRect(const math::RectF& rectf) {
  // We convert from floating point to integer in such a way that two boxes
  // joined at the seams in float-space continue to be joined at the seams in
  // integer-space.
  int x = RoundToInt(rectf.x());
  int y = RoundToInt(rectf.y());

  return math::Rect(x, y, RoundToInt(rectf.right()) - x,
                    RoundToInt(rectf.bottom()) - y);
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
