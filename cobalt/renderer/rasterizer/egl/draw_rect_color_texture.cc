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

#include "cobalt/renderer/rasterizer/egl/draw_rect_color_texture.h"

#include <GLES2/gl2.h>

#include "base/basictypes.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "egl/generated_shader_impl.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
struct VertexAttributes {
  float position[3];
  float texcoord[2];
  uint32_t color;
};
}  // namespace

DrawRectColorTexture::DrawRectColorTexture(GraphicsState* graphics_state,
    const BaseState& base_state,
    const math::RectF& rect, const render_tree::ColorRGBA& color,
    const backend::TextureEGL* texture,
    const math::Matrix3F& texcoord_transform,
    bool clamp_texcoords)
    : DrawObject(base_state),
      texcoord_transform_(texcoord_transform),
      rect_(rect),
      texture_(texture),
      vertex_buffer_(NULL),
      clamp_texcoords_(clamp_texcoords),
      tile_texture_(false) {
  color_ = GetGLRGBA(color * base_state_.opacity);
  graphics_state->ReserveVertexData(4 * sizeof(VertexAttributes));
}

DrawRectColorTexture::DrawRectColorTexture(GraphicsState* graphics_state,
    const BaseState& base_state,
    const math::RectF& rect, const render_tree::ColorRGBA& color,
    const backend::TextureEGL* texture,
    const math::Matrix3F& texcoord_transform,
    const base::Closure& draw_offscreen,
    const base::Closure& draw_onscreen)
    : DrawObject(base_state),
      texcoord_transform_(texcoord_transform),
      rect_(rect),
      texture_(texture),
      draw_offscreen_(draw_offscreen),
      draw_onscreen_(draw_onscreen),
      vertex_buffer_(NULL),
      clamp_texcoords_(false),
      tile_texture_(false) {
  color_ = GetGLRGBA(color * base_state_.opacity);
  graphics_state->ReserveVertexData(4 * sizeof(VertexAttributes));
}

void DrawRectColorTexture::ExecuteOffscreenRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  if (!draw_offscreen_.is_null()) {
    draw_offscreen_.Run();
  }
}

void DrawRectColorTexture::ExecuteOnscreenUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  VertexAttributes attributes[4] = {
    { { rect_.x(), rect_.bottom(), base_state_.depth },      // uv = (0,1)
      { texcoord_transform_(0, 1) + texcoord_transform_(0, 2),
        texcoord_transform_(1, 1) + texcoord_transform_(1, 2) }, color_ },
    { { rect_.right(), rect_.bottom(), base_state_.depth },  // uv = (1,1)
      { texcoord_transform_(0, 0) + texcoord_transform_(0, 1) +
          texcoord_transform_(0, 2),
        texcoord_transform_(1, 0) + texcoord_transform_(1, 1) +
          texcoord_transform_(1, 2) }, color_ },
    { { rect_.right(), rect_.y(), base_state_.depth },       // uv = (1,0)
      { texcoord_transform_(0, 0) + texcoord_transform_(0, 2),
        texcoord_transform_(1, 0) + texcoord_transform_(1, 2) }, color_ },
    { { rect_.x(), rect_.y(), base_state_.depth },           // uv = (0,0)
      { texcoord_transform_(0, 2), texcoord_transform_(1, 2) }, color_ },
  };
  COMPILE_ASSERT(sizeof(attributes) == 4 * sizeof(VertexAttributes),
                 bad_padding);
  vertex_buffer_ = graphics_state->AllocateVertexData(
      sizeof(attributes));
  SbMemoryCopy(vertex_buffer_, attributes, sizeof(attributes));

  // Find minimum and maximum texcoord values.
  texcoord_clamp_[0] = attributes[0].texcoord[0];
  texcoord_clamp_[1] = attributes[0].texcoord[1];
  texcoord_clamp_[2] = attributes[0].texcoord[0];
  texcoord_clamp_[3] = attributes[0].texcoord[1];
  for (int i = 1; i < arraysize(attributes); ++i) {
    float texcoord_u = attributes[i].texcoord[0];
    float texcoord_v = attributes[i].texcoord[1];
    if (texcoord_clamp_[0] > texcoord_u) {
      texcoord_clamp_[0] = texcoord_u;
    } else if (texcoord_clamp_[2] < texcoord_u) {
      texcoord_clamp_[2] = texcoord_u;
    }
    if (texcoord_clamp_[1] > texcoord_v) {
      texcoord_clamp_[1] = texcoord_v;
    } else if (texcoord_clamp_[3] < texcoord_v) {
      texcoord_clamp_[3] = texcoord_v;
    }
  }

  tile_texture_ = texcoord_clamp_[0] < 0.0f || texcoord_clamp_[1] < 0.0f ||
                  texcoord_clamp_[2] > 1.0f || texcoord_clamp_[3] > 1.0f;

  if (clamp_texcoords_) {
    // Inset 0.5-epsilon so the border texels are still sampled, but nothing
    // beyond.
    const float kTexelInset = 0.499f;
    texcoord_clamp_[0] += kTexelInset / texture_->GetSize().width();
    texcoord_clamp_[1] += kTexelInset / texture_->GetSize().height();
    texcoord_clamp_[2] -= kTexelInset / texture_->GetSize().width();
    texcoord_clamp_[3] -= kTexelInset / texture_->GetSize().height();
  }
}

void DrawRectColorTexture::ExecuteOnscreenRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  if (!draw_onscreen_.is_null()) {
    draw_onscreen_.Run();
  }

  ShaderProgram<ShaderVertexColorTexcoord,
                ShaderFragmentColorTexcoord>* program;
  program_manager->GetProgram(&program);
  graphics_state->UseProgram(program->GetHandle());
  graphics_state->UpdateClipAdjustment(
      program->GetVertexShader().u_clip_adjustment());
  graphics_state->UpdateTransformMatrix(
      program->GetVertexShader().u_view_matrix(),
      base_state_.transform);
  graphics_state->Scissor(base_state_.scissor.x(), base_state_.scissor.y(),
      base_state_.scissor.width(), base_state_.scissor.height());
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_position(), 3, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, position));
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_color(), 4, GL_UNSIGNED_BYTE, GL_TRUE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, color));
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_texcoord(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, texcoord));
  graphics_state->VertexAttribFinish();
  GL_CALL(glUniform4fv(program->GetFragmentShader().u_texcoord_clamp(),
      1, texcoord_clamp_));

  if (tile_texture_) {
    graphics_state->ActiveBindTexture(
        program->GetFragmentShader().u_texture_texunit(),
        texture_->GetTarget(), texture_->gl_handle(), GL_REPEAT);
    GL_CALL(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
    graphics_state->ActiveBindTexture(
        program->GetFragmentShader().u_texture_texunit(),
        texture_->GetTarget(), texture_->gl_handle(), GL_CLAMP_TO_EDGE);
  } else {
    graphics_state->ActiveBindTexture(
        program->GetFragmentShader().u_texture_texunit(),
        texture_->GetTarget(), texture_->gl_handle());
    GL_CALL(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
