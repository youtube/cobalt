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

#include "cobalt/renderer/rasterizer/egl/draw_rrect_color_texture.h"

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
const int kVertexCount = 4;

struct VertexAttributes {
  float position[2];
  float offset[2];
  float texcoord[2];
};

void SetVertex(VertexAttributes* vertex, float x, float y, float u, float v) {
  vertex->position[0] = x;
  vertex->position[1] = y;
  vertex->offset[0] = x;
  vertex->offset[1] = y;
  vertex->texcoord[0] = u;
  vertex->texcoord[1] = v;
}
}  // namespace

DrawRRectColorTexture::DrawRRectColorTexture(GraphicsState* graphics_state,
    const BaseState& base_state, const math::RectF& rect,
    const render_tree::ColorRGBA& color, const backend::TextureEGL* texture,
    const math::Matrix3F& texcoord_transform,
    bool clamp_texcoords)
    : DrawObject(base_state),
      texcoord_transform_(texcoord_transform),
      rect_(rect),
      texture_(texture),
      vertex_buffer_(NULL),
      clamp_texcoords_(clamp_texcoords),
      tile_texture_(false) {
  DCHECK(base_state_.rounded_scissor_corners);
  color_ = GetDrawColor(color) * base_state_.opacity;
  graphics_state->ReserveVertexData(kVertexCount * sizeof(VertexAttributes));
}

void DrawRRectColorTexture::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  VertexAttributes attributes[kVertexCount];
  SetVertex(&attributes[0], rect_.x(), rect_.y(),
      texcoord_transform_(0, 2), texcoord_transform_(1, 2));    // uv = (0,0)
  SetVertex(&attributes[1], rect_.right(), rect_.y(),
      texcoord_transform_(0, 0) + texcoord_transform_(0, 2),    // uv = (1,0)
      texcoord_transform_(1, 0) + texcoord_transform_(1, 2));
  SetVertex(&attributes[2], rect_.right(), rect_.bottom(),
      texcoord_transform_(0, 0) + texcoord_transform_(0, 1) +   // uv = (1,1)
          texcoord_transform_(0, 2),
      texcoord_transform_(1, 0) + texcoord_transform_(1, 1) +
          texcoord_transform_(1, 2));
  SetVertex(&attributes[3], rect_.x(), rect_.bottom(),
      texcoord_transform_(0, 1) + texcoord_transform_(0, 2),    // uv = (0,1)
      texcoord_transform_(1, 1) + texcoord_transform_(1, 2));
  vertex_buffer_ = graphics_state->AllocateVertexData(sizeof(attributes));
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

void DrawRRectColorTexture::ExecuteRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  ShaderProgram<ShaderVertexOffsetTexcoord,
                ShaderFragmentTexcoordColorRrect>* program;
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
      program->GetVertexShader().a_position(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, position));
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_offset(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, offset));
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_texcoord(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, texcoord));
  graphics_state->VertexAttribFinish();

  SetRRectUniforms(program->GetFragmentShader().u_rect(),
                   program->GetFragmentShader().u_corners(),
                   base_state_.rounded_scissor_rect,
                   *base_state_.rounded_scissor_corners, 0.5f);
  GL_CALL(glUniform4f(program->GetFragmentShader().u_color(),
                      color_.r(), color_.g(), color_.b(), color_.a()));
  GL_CALL(glUniform4fv(program->GetFragmentShader().u_texcoord_clamp(), 1,
                       texcoord_clamp_));

  if (tile_texture_) {
    graphics_state->ActiveBindTexture(
        program->GetFragmentShader().u_texture_texunit(),
        texture_->GetTarget(), texture_->gl_handle(), GL_REPEAT);
    GL_CALL(glDrawArrays(GL_TRIANGLE_FAN, 0, kVertexCount));
    graphics_state->ActiveBindTexture(
        program->GetFragmentShader().u_texture_texunit(),
        texture_->GetTarget(), texture_->gl_handle(), GL_CLAMP_TO_EDGE);
  } else {
    graphics_state->ActiveBindTexture(
        program->GetFragmentShader().u_texture_texunit(),
        texture_->GetTarget(), texture_->gl_handle());
    GL_CALL(glDrawArrays(GL_TRIANGLE_FAN, 0, kVertexCount));
  }
}

base::TypeId DrawRRectColorTexture::GetTypeId() const {
  return ShaderProgram<ShaderVertexOffsetTexcoord,
                       ShaderFragmentTexcoordColorRrect>::GetTypeId();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
