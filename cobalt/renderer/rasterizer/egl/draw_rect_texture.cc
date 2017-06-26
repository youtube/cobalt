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

#include "cobalt/renderer/rasterizer/egl/draw_rect_texture.h"

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
  float position[2];
  float texcoord[2];
};
}  // namespace

DrawRectTexture::DrawRectTexture(GraphicsState* graphics_state,
    const BaseState& base_state,
    const math::RectF& rect,
    const backend::TextureEGL* texture,
    const math::Matrix3F& texcoord_transform)
    : DrawObject(base_state),
      texcoord_transform_(texcoord_transform),
      rect_(rect),
      texture_(texture),
      vertex_buffer_(NULL),
      tile_texture_(false) {
  graphics_state->ReserveVertexData(4 * sizeof(VertexAttributes));
}

void DrawRectTexture::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  VertexAttributes attributes[4] = {
    { { rect_.x(), rect_.bottom() },      // uv = (0,1)
      { texcoord_transform_(0, 1) + texcoord_transform_(0, 2),
        texcoord_transform_(1, 1) + texcoord_transform_(1, 2) } },
    { { rect_.right(), rect_.bottom() },  // uv = (1,1)
      { texcoord_transform_(0, 0) + texcoord_transform_(0, 1) +
          texcoord_transform_(0, 2),
        texcoord_transform_(1, 0) + texcoord_transform_(1, 1) +
          texcoord_transform_(1, 2) } },
    { { rect_.right(), rect_.y() },       // uv = (1,0)
      { texcoord_transform_(0, 0) + texcoord_transform_(0, 2),
        texcoord_transform_(1, 0) + texcoord_transform_(1, 2) } },
    { { rect_.x(), rect_.y() },           // uv = (0,0)
      { texcoord_transform_(0, 2), texcoord_transform_(1, 2) } },
  };
  COMPILE_ASSERT(sizeof(attributes) == 4 * sizeof(VertexAttributes),
                 bad_padding);
  vertex_buffer_ = graphics_state->AllocateVertexData(
      sizeof(attributes));
  SbMemoryCopy(vertex_buffer_, attributes, sizeof(attributes));

  for (int i = 0; i < arraysize(attributes); ++i) {
    tile_texture_ = tile_texture_ ||
        attributes[i].texcoord[0] < 0.0f || attributes[i].texcoord[0] > 1.0f ||
        attributes[i].texcoord[1] < 0.0f || attributes[i].texcoord[1] > 1.0f;
  }
}

void DrawRectTexture::ExecuteRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  ShaderProgram<ShaderVertexTexcoord,
                ShaderFragmentTexcoord>* program;
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
      program->GetVertexShader().a_texcoord(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, texcoord));
  graphics_state->VertexAttribFinish();

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

base::TypeId DrawRectTexture::GetTypeId() const {
  return ShaderProgram<ShaderVertexTexcoord,
                       ShaderFragmentTexcoord>::GetTypeId();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
