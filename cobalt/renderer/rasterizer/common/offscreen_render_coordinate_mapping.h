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

// The offscreen-render coordinate mapping logic here contains helper logic
// common to all rasterizers that need to perform sub-rendering to offscreen
// surfaces which will then be subsequently rendered into an outer surface.  The
// OffscreenRenderCoordinateMapping structure contains all information needed to
// tell rendering calls where to draw into the offscreen surface, and then
// where to draw that offscreen surface into the larger surface.

#ifndef COBALT_RENDERER_RASTERIZER_COMMON_OFFSCREEN_RENDER_COORDINATE_MAPPING_H_
#define COBALT_RENDERER_RASTERIZER_COMMON_OFFSCREEN_RENDER_COORDINATE_MAPPING_H_

#include "base/optional.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {

struct OffscreenRenderCoordinateMapping {
  OffscreenRenderCoordinateMapping()
      : sub_render_transform(math::Matrix3F::Zeros()) {}

  OffscreenRenderCoordinateMapping(const math::Rect& output_bounds,
                                   const math::Vector2dF& output_pre_translate,
                                   const math::Vector2dF& output_post_scale,
                                   const math::Matrix3F& sub_render_transform)
      : output_bounds(output_bounds),
        output_pre_translate(output_pre_translate),
        output_post_scale(output_post_scale),
        sub_render_transform(sub_render_transform) {}

  // The resulting bounding box into which the sub-rendered results should be
  // rendered, rounded to integer bounds (since the sub-render will be rendered
  // onto a surface with integer dimensions).
  math::Rect output_bounds;

  // A translation that should be pre multiplied to the current transform (the
  // |transform| parameter passed into GetOffscreenRenderCoordinateMapping())
  // before rendering the sub-rendered offscreen surface into the outer surface.
  math::Vector2dF output_pre_translate;

  // A scale that should be post multiplied to the current transform before
  // rendering the sub-rendered offscreen surface into the outer surface.
  math::Vector2dF output_post_scale;

  // The transform to be applied when performing the sub-render.  This will
  // for example set the origin such that the contents of the sub-render are
  // packed into the corner.
  math::Matrix3F sub_render_transform;
};

// This function takes a |local_bounds| parameter indicating the local bounding
// box of the content to be sub-rendered, and a |transform| indicating any
// transforms that should be applied to the local_bounds in order to obtain
// an absolute bounds.  |viewport_bounds| will be used to clip the resulting
// offscreen surface such that only what is visible will be rendered.
// |align_transform| specifies whether or not the fractional component of the
// translation should be aligned between the offscreen-render and the output
// render.  This may not be possible however if |transform| includes rotations
// or sheers.
// The resulting data can be used to setup a sub-surface for offscreen rendering
// which can then be subsequently rendered to the main render target.
OffscreenRenderCoordinateMapping GetOffscreenRenderCoordinateMapping(
    const math::RectF& local_bounds, const math::Matrix3F& transform,
    const base::Optional<math::Rect>& viewport_bounds);

}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_COMMON_OFFSCREEN_RENDER_COORDINATE_MAPPING_H_
