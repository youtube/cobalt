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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_CONVERSIONS_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_CONVERSIONS_H_

#include <vector>
#include "third_party/skia/include/core/SkColor.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

inline SkColor ToSkColor(const render_tree::ColorRGBA& color) {
  return SkColorSetARGB(color.rgb8_a(), color.rgb8_r(), color.rgb8_g(),
                        color.rgb8_b());
}

// Helper struct to convert render_tree::ColorStopList to a format that Skia
// methods will easily accept.
struct SkiaColorStops {
  explicit SkiaColorStops(const render_tree::ColorStopList& color_stops)
      : colors(color_stops.size()),
        positions(color_stops.size()),
        has_alpha(false) {
    for (size_t i = 0; i < color_stops.size(); ++i) {
      if (color_stops[i].color.a() < 1.0f) {
        has_alpha = true;
      }

      colors[i] = ToSkColor(color_stops[i].color);
      positions[i] = color_stops[i].position;
    }
  }

  std::size_t size() const {
    DCHECK(colors.size() == positions.size());
    return colors.size();
  }

  std::vector<SkColor> colors;
  std::vector<SkScalar> positions;
  bool has_alpha;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_CONVERSIONS_H_
