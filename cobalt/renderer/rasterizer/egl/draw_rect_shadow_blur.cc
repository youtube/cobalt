// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/egl/draw_rect_shadow_blur.h"

#include <GLES2/gl2.h>
#include <algorithm>

#include "base/logging.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
const float kSqrt2 = 1.414213562f;
const float kSqrtPi = 1.772453851f;

// The blur kernel extends for a limited number of sigmas.
const float kBlurExtentInSigmas = 3.0f;

// The error function is used to calculate the gaussian blur. Inputs for
// the error function should be scaled by 1 / (sqrt(2) * sigma). Alternatively,
// it can be viewed as kBlurDistance is the input value at which the blur
// intensity is effectively 0.
const float kBlurDistance = kBlurExtentInSigmas / kSqrt2;

void SetBlurRRectUniforms(const ShaderFragmentColorBlurRrects& shader,
    math::RectF rect, render_tree::RoundedCorners corners, float sigma) {
  // Ensure a minimum radius for each corner to avoid division by zero.
  const float kMinSize = 0.01f;

  corners = corners.Inset(-kMinSize, -kMinSize, -kMinSize, -kMinSize);
  rect.Outset(kMinSize, kMinSize);

  // Specify the blur extent size and the (min.y, max.y) for the rect.
  const float kBlurExtentInPixels = kBlurExtentInSigmas * sigma;
  GL_CALL(glUniform3f(shader.u_blur_extent(),
                      kBlurExtentInPixels, rect.y(), rect.bottom()));

  // Set the "start" and "scale" values so that (pos - start) * scale is in the
  // first quadrant and normalized. Then specify "radius" so that normalized *
  // radius + start specifies a point on the respective corner.
  GL_CALL(glUniform4f(shader.u_spread_start_x(),
                      rect.x() + corners.top_left.horizontal,
                      rect.right() - corners.top_right.horizontal,
                      rect.x() + corners.bottom_left.horizontal,
                      rect.right() - corners.bottom_right.horizontal));
  GL_CALL(glUniform4f(shader.u_spread_start_y(),
                      rect.y() + corners.top_left.vertical,
                      rect.y() + corners.top_right.vertical,
                      rect.bottom() - corners.bottom_left.vertical,
                      rect.bottom() - corners.bottom_right.vertical));
  GL_CALL(glUniform4f(shader.u_spread_scale_y(),
                      -1.0f / corners.top_left.vertical,
                      -1.0f / corners.top_right.vertical,
                      1.0f / corners.bottom_left.vertical,
                      1.0f / corners.bottom_right.vertical));
  GL_CALL(glUniform4f(shader.u_spread_radius_x(),
                      -corners.top_left.horizontal,
                      corners.top_right.horizontal,
                      -corners.bottom_left.horizontal,
                      corners.bottom_right.horizontal));
}
}  // namespace

DrawRectShadowBlur::DrawRectShadowBlur(GraphicsState* graphics_state,
    const BaseState& base_state, const math::RectF& base_rect,
    const OptionalRoundedCorners& base_corners, const math::RectF& spread_rect,
    const OptionalRoundedCorners& spread_corners,
    const render_tree::ColorRGBA& color, float blur_sigma, bool inset)
    : DrawRectShadowSpread(graphics_state, base_state),
      spread_rect_(spread_rect),
      spread_corners_(spread_corners),
      blur_sigma_(blur_sigma),
      is_inset_(inset) {
  const float kBlurExtentInPixels = kBlurExtentInSigmas * blur_sigma;

  if (inset) {
    outer_rect_ = base_rect;
    outer_corners_ = base_corners;
    inner_rect_ = spread_rect;
    inner_rect_.Inset(kBlurExtentInPixels, kBlurExtentInPixels);
    if (inner_rect_.IsEmpty()) {
      inner_rect_.set_origin(spread_rect.CenterPoint());
    }
    if (spread_corners) {
      inner_corners_ = spread_corners->Inset(kBlurExtentInPixels,
          kBlurExtentInPixels, kBlurExtentInPixels, kBlurExtentInPixels);
    }
  } else {
    inner_rect_ = base_rect;
    inner_corners_ = base_corners;
    outer_rect_ = spread_rect;
    outer_rect_.Outset(kBlurExtentInPixels, kBlurExtentInPixels);
    if (spread_corners) {
      outer_corners_ = spread_corners->Inset(-kBlurExtentInPixels,
          -kBlurExtentInPixels, -kBlurExtentInPixels, -kBlurExtentInPixels);
    }
  }

  if (base_corners || spread_corners) {
    // If rounded rects are specified, then both the base and spread rects
    // must have rounded corners.
    DCHECK(inner_corners_);
    DCHECK(outer_corners_);
    DCHECK(spread_corners_);
  } else {
    // Non-rounded rects specify vertex offset in terms of sigma from the
    // center of the spread rect.
    offset_scale_ = kBlurDistance / kBlurExtentInPixels;
    offset_center_ = spread_rect_.CenterPoint();
  }

  color_ = GetGLRGBA(color * base_state_.opacity);
}

void DrawRectShadowBlur::ExecuteRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  // Draw the blurred shadow.
  if (inner_corners_) {
    ShaderProgram<CommonVertexShader,
                  ShaderFragmentColorBlurRrects>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupShader(program->GetVertexShader(), graphics_state);

    float sigma_scale = kBlurDistance / (kBlurExtentInSigmas * blur_sigma_);
    GL_CALL(glUniform2f(program->GetFragmentShader().u_sigma_scale(),
                        sigma_scale, sigma_scale));

    // Pre-calculate the scale values to calculate the normalized gaussian.
    GL_CALL(glUniform2f(program->GetFragmentShader().u_gaussian_scale(),
                        -1.0f / (2.0f * blur_sigma_ * blur_sigma_),
                        1.0f / (kSqrt2 * kSqrtPi * blur_sigma_)));
    if (is_inset_) {
      // Set the outer rect to be an inclusive scissor, and invert the shadow.
      SetRRectUniforms(program->GetFragmentShader().u_scissor_rect(),
                       program->GetFragmentShader().u_scissor_corners(),
                       outer_rect_, *outer_corners_, 0.5f);
      GL_CALL(glUniform2f(program->GetFragmentShader().u_scale_add(),
                          -1.0f, 1.0f));
    } else {
      // Set the inner rect to be an exclusive scissor.
      SetRRectUniforms(program->GetFragmentShader().u_scissor_rect(),
                       program->GetFragmentShader().u_scissor_corners(),
                       inner_rect_, *inner_corners_, 0.5f);
      GL_CALL(glUniform2f(program->GetFragmentShader().u_scale_add(),
                          1.0f, 0.0f));
    }
    SetBlurRRectUniforms(program->GetFragmentShader(),
                         spread_rect_, *spread_corners_, blur_sigma_);
  } else {
    ShaderProgram<CommonVertexShader,
                  ShaderFragmentColorBlur>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupShader(program->GetVertexShader(), graphics_state);
    if (is_inset_) {
      // Invert the shadow.
      GL_CALL(glUniform2f(program->GetFragmentShader().u_scale_add(),
                          -1.0f, 1.0f));
    } else {
      // Keep the normal (outset) shadow.
      GL_CALL(glUniform2f(program->GetFragmentShader().u_scale_add(),
                          1.0f, 0.0f));
    }
    GL_CALL(glUniform4f(program->GetFragmentShader().u_blur_rect(),
        (spread_rect_.x() - offset_center_.x()) * offset_scale_,
        (spread_rect_.y() - offset_center_.y()) * offset_scale_,
        (spread_rect_.right() - offset_center_.x()) * offset_scale_,
        (spread_rect_.bottom() - offset_center_.y()) * offset_scale_));
  }

  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, vertex_count_));
}

base::TypeId DrawRectShadowBlur::GetTypeId() const {
  if (inner_corners_) {
    return ShaderProgram<CommonVertexShader,
                         ShaderFragmentColorBlurRrects>::GetTypeId();
  } else {
    return ShaderProgram<CommonVertexShader,
                         ShaderFragmentColorBlur>::GetTypeId();
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
