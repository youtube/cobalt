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

#include "cobalt/renderer/rasterizer/common/offscreen_render_coordinate_mapping.h"

#include "cobalt/math/transform_2d.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {

using math::Matrix3F;
using math::Rect;
using math::RectF;
using math::Vector2dF;
using math::Vector3dF;
using math::TranslateMatrix;
using math::ScaleMatrix;

namespace {
float GetFractionalComponent(float value) { return value - std::floor(value); }
}  // namespace

OffscreenRenderCoordinateMapping GetOffscreenRenderCoordinateMapping(
    const RectF& local_bounds, const Matrix3F& transform,
    const base::Optional<Rect>& viewport_bounds) {
  bool has_rotation_or_skew =
      transform.Get(0, 1) != 0 || transform.Get(1, 0) != 0;

  // We decide at this point whether to attempt to align the fractional value of
  // our content's translation within the offscreen surface such that it would
  // be equal to what it would be in the outer surface.  If we are rotating
  // or skewing, it is not useful to do this so we don't bother in that case.
  bool align_translate = !has_rotation_or_skew;

  // We extract the scale from the transform and let that affect the size of our
  // output bounding box so that our offscreen render is in the same resolution
  // as its outer surface area.
  Vector3dF column_0(transform.column(0));
  Vector3dF column_1(transform.column(1));
  Vector2dF scale(column_0.Length(), column_1.Length());

  // If we're aligning translate, bring the fractional component of the
  // translation into the offscreen render so that we can ensure that we are
  // sub-pixel aligned when rendering it.
  Vector2dF translate;
  if (align_translate) {
    translate.SetVector(GetFractionalComponent(transform.Get(0, 2)),
                        GetFractionalComponent(transform.Get(1, 2)));
  }

  // |scaled_local_bounds| represents |local_bounds| transformed into a hybrid
  // local/viewport space where only scale (and possibly translate) components
  // of |transform| are applied.  This is the space in which we will rasterize
  // to the offscreen surface.
  RectF scaled_local_bounds = local_bounds;
  scaled_local_bounds.Scale(scale.x(), scale.y());
  scaled_local_bounds += translate;

  // Since our offscreen render is already taking into account scale (and
  // possibly translation), we communicate to the caller that they should not
  // apply these components of the transformation when rendering the offscreen
  // surface.
  Vector2dF output_pre_translate(-translate.x(), -translate.y());
  Vector2dF output_post_scale(1.0f / scale.x(), 1.0f / scale.y());

  // To avoid rendering anything we don't need to render, intersect the
  // bounding box with our current viewport bounds.  We do this in the scaled
  // local bounds space since that is where our rendering will occur, thus
  // we must take the inverse of the components of the transform not reflected
  // in scaled local bounds space.
  RectF clipped_scaled_local_bounds;
  if (viewport_bounds) {
    Matrix3F inverse_transform = (TranslateMatrix(output_pre_translate) *
                                  transform * ScaleMatrix(output_post_scale))
                                     .Inverse();
    clipped_scaled_local_bounds = IntersectRects(
        inverse_transform.MapRect(*viewport_bounds), scaled_local_bounds);
  } else {
    clipped_scaled_local_bounds = scaled_local_bounds;
  }

  // Round out our RectF to a Rect here to get integer coordinates that can
  // be used to define offscreen surface dimensions.
  Rect output_bounds = math::RoundOut(clipped_scaled_local_bounds);

  // Now determine the offscreen transform that should be used when rendering
  // the offscreen render tree to an offscreen surface.
  // The |translate| component is already present in |output_bounds|, so
  // we remove it here.  Ultimately we will be left with only the fractional
  // part of the |translate| component.
  float offscreen_translate_x = translate.x() - output_bounds.origin().x();
  float offscreen_translate_y = translate.y() - output_bounds.origin().y();

  Matrix3F offscreen_transform =
      Matrix3F::FromValues(scale.x(), 0, offscreen_translate_x, 0, scale.y(),
                           offscreen_translate_y, 0, 0, 1);

  return OffscreenRenderCoordinateMapping(output_bounds, output_pre_translate,
                                          output_post_scale,
                                          offscreen_transform);
}

}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
