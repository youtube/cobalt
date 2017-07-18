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
// The blur kernel extends for a limited number of sigmas.
const float kBlurExtentInSigmas = 3.0f;

// The gaussian integral formula used to calculate blur intensity reaches 0
// intensity at a distance of 1.5. To express pixels in terms of input for
// this function, a distance of kBlurExtentInSigmas * |blur_sigma| should equal
// kBlurDistance.
const float kBlurDistance = 1.5f;

void SetSigmaTweak(GLint sigma_tweak_uniform, const math::RectF& spread_rect,
      const DrawObject::OptionalRoundedCorners& spread_corners,
      float blur_sigma) {
  const float kBlurExtentInPixels = kBlurExtentInSigmas * blur_sigma;

  // If the blur kernel is larger than the spread rect, then adjust the input
  // values to the blur calculations to better match the reference output.
  const float kTweakScale = 0.15f;
  float sigma_tweak[2] = {
    std::max(kBlurExtentInPixels - spread_rect.width(), 0.0f) *
        kTweakScale / blur_sigma,
    std::max(kBlurExtentInPixels - spread_rect.height(), 0.0f) *
        kTweakScale / blur_sigma,
  };

  if (spread_corners) {
    // The shader for blur with rounded rects does not fully factor in
    // proximity to rounded corners. For example, given a 15-pixel wide
    // spread rect with two neighboring rounded corners with radius 7,
    // the center pixels would not include the nearby corners in the
    // calculation of distance to the edge -- it would always be 7.
    float corner_gap[2] = {
      std::min(spread_rect.width() - spread_corners->top_left.horizontal
                                   - spread_corners->top_right.horizontal,
               spread_rect.width() - spread_corners->bottom_left.horizontal
                                   - spread_corners->bottom_right.horizontal),
      std::min(spread_rect.height() - spread_corners->top_left.vertical
                                    - spread_corners->bottom_left.vertical,
               spread_rect.height() - spread_corners->top_right.vertical
                                    - spread_corners->bottom_right.vertical),
    };

    // This tweak is limited because each edge can have different-sized
    // gaps between corners. However, the shader is already pretty large,
    // so this tweak will do for now. The situation is only noticeable with
    // relatively large blur sigmas.
    const float kCornerTweakScale = 0.03f;
    sigma_tweak[0] += std::max(kBlurExtentInPixels - corner_gap[0], 0.0f) *
        kCornerTweakScale / blur_sigma;
    sigma_tweak[1] += std::max(kBlurExtentInPixels - corner_gap[1], 0.0f) *
        kCornerTweakScale / blur_sigma;
  }

  GL_CALL(glUniform2fv(sigma_tweak_uniform, 1, sigma_tweak));
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
    inner_corners_->Normalize(inner_rect_);
    outer_corners_->Normalize(outer_rect_);
    spread_corners_->Normalize(spread_rect_);
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
    SetSigmaTweak(program->GetFragmentShader().u_sigma_tweak(),
                  spread_rect_, spread_corners_, blur_sigma_);
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
    SetRRectUniforms(program->GetFragmentShader().u_spread_rect(),
                     program->GetFragmentShader().u_spread_corners(),
                     spread_rect_, *spread_corners_, 0.0f);
  } else {
    ShaderProgram<CommonVertexShader,
                  ShaderFragmentColorBlur>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupShader(program->GetVertexShader(), graphics_state);

    SetSigmaTweak(program->GetFragmentShader().u_sigma_tweak(),
                  spread_rect_, spread_corners_, blur_sigma_);
    if (is_inset_) {
      // Invert the shadow.
      GL_CALL(glUniform2f(program->GetFragmentShader().u_scale_add(),
                          -1.0f, 1.0f));
    } else {
      // Keep the normal (outset) shadow.
      GL_CALL(glUniform2f(program->GetFragmentShader().u_scale_add(),
                          1.0f, 0.0f));
    }
    GL_CALL(glUniform2f(program->GetFragmentShader().u_blur_radius(),
                        spread_rect_.width() * 0.5f * offset_scale_,
                        spread_rect_.height() * 0.5f * offset_scale_));
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
