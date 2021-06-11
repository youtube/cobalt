// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/rasterizer/egl/draw_rect_texture.h"

#include "base/basictypes.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
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
      textures_{texture, NULL, NULL},
      vertex_buffer_(NULL),
      tile_texture_(false) {
  DCHECK(textures_[0]);
  graphics_state->ReserveVertexData(4 * sizeof(VertexAttributes));
}

DrawRectTexture::DrawRectTexture(
    GraphicsState* graphics_state, const BaseState& base_state,
    const math::RectF& rect, const backend::TextureEGL* y_texture,
    const backend::TextureEGL* u_texture, const backend::TextureEGL* v_texture,
    const float (&color_transform_in_column_major)[16],
    const math::Matrix3F& texcoord_transform)
    : DrawObject(base_state),
      texcoord_transform_(texcoord_transform),
      rect_(rect),
      textures_{y_texture, u_texture, v_texture},
      vertex_buffer_(NULL),
      tile_texture_(false) {
  DCHECK(textures_[0]);
  DCHECK(textures_[1]);
  DCHECK(textures_[2]);
  static_assert(
      sizeof(color_transform_) == sizeof(color_transform_in_column_major),
      "color_transform_ and color_transform_in_column_major size mismatch");

  memcpy(color_transform_, color_transform_in_column_major,
               sizeof(color_transform_));
  graphics_state->ReserveVertexData(4 * sizeof(VertexAttributes));
}

void DrawRectTexture::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  VertexAttributes attributes[4] = {
      {{rect_.x(), rect_.bottom()},  // uv = (0,1)
       {texcoord_transform_(0, 1) + texcoord_transform_(0, 2),
        texcoord_transform_(1, 1) + texcoord_transform_(1, 2)}},
      {{rect_.right(), rect_.bottom()},  // uv = (1,1)
       {texcoord_transform_(0, 0) + texcoord_transform_(0, 1) +
            texcoord_transform_(0, 2),
        texcoord_transform_(1, 0) + texcoord_transform_(1, 1) +
            texcoord_transform_(1, 2)}},
      {{rect_.right(), rect_.y()},  // uv = (1,0)
       {texcoord_transform_(0, 0) + texcoord_transform_(0, 2),
        texcoord_transform_(1, 0) + texcoord_transform_(1, 2)}},
      {{rect_.x(), rect_.y()},  // uv = (0,0)
       {texcoord_transform_(0, 2), texcoord_transform_(1, 2)}},
  };
  COMPILE_ASSERT(sizeof(attributes) == 4 * sizeof(VertexAttributes),
                 bad_padding);
  vertex_buffer_ = graphics_state->AllocateVertexData(sizeof(attributes));
  memcpy(vertex_buffer_, attributes, sizeof(attributes));

  for (int i = 0; i < arraysize(attributes); ++i) {
    tile_texture_ = tile_texture_ || attributes[i].texcoord[0] < 0.0f ||
                    attributes[i].texcoord[0] > 1.0f ||
                    attributes[i].texcoord[1] < 0.0f ||
                    attributes[i].texcoord[1] > 1.0f;
  }
}

void DrawRectTexture::SetupVertexShader(
    GraphicsState* graphics_state, const ShaderVertexTexcoord& vertex_shader) {
  graphics_state->UpdateClipAdjustment(vertex_shader.u_clip_adjustment());
  graphics_state->UpdateTransformMatrix(vertex_shader.u_view_matrix(),
                                        base_state_.transform);
  graphics_state->Scissor(base_state_.scissor.x(), base_state_.scissor.y(),
                          base_state_.scissor.width(),
                          base_state_.scissor.height());
  graphics_state->VertexAttribPointer(
      vertex_shader.a_position(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes),
      vertex_buffer_ + offsetof(VertexAttributes, position));
  graphics_state->VertexAttribPointer(
      vertex_shader.a_texcoord(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes),
      vertex_buffer_ + offsetof(VertexAttributes, texcoord));
  graphics_state->VertexAttribFinish();
}

template <typename FragmentShader>
void DrawRectTexture::SetupFragmentShaderAndDraw(
    GraphicsState* graphics_state, const FragmentShader& fragment_shader) {
  for (int i = 0; i < SB_ARRAY_SIZE_INT(textures_); ++i) {
    if (textures_[i] == NULL) {
      break;
    }

    if (tile_texture_) {
      graphics_state->ActiveBindTexture(fragment_shader.u_texture_texunit(i),
                                        textures_[i]->GetTarget(),
                                        textures_[i]->gl_handle(), GL_REPEAT);
    } else {
      graphics_state->ActiveBindTexture(fragment_shader.u_texture_texunit(i),
                                        textures_[i]->GetTarget(),
                                        textures_[i]->gl_handle());
    }
  }

  GL_CALL(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));

  if (!tile_texture_) {
    return;
  }

  for (int i = 0; i < SB_ARRAY_SIZE_INT(textures_); ++i) {
    if (textures_[i] == NULL) {
      break;
    }

    graphics_state->ActiveBindTexture(
        fragment_shader.u_texture_texunit(i), textures_[i]->GetTarget(),
        textures_[i]->gl_handle(), GL_CLAMP_TO_EDGE);
  }
}

void DrawRectTexture::ExecuteRasterize(GraphicsState* graphics_state,
                                       ShaderProgramManager* program_manager) {
  if (textures_[1] == NULL) {
    ShaderProgram<ShaderVertexTexcoord, ShaderFragmentTexcoord>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupVertexShader(graphics_state, program->GetVertexShader());
    SetupFragmentShaderAndDraw(graphics_state, program->GetFragmentShader());
  } else {
    ShaderProgram<ShaderVertexTexcoord, ShaderFragmentTexcoordYuv3>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupVertexShader(graphics_state, program->GetVertexShader());
    GL_CALL(glUniformMatrix4fv(
        program->GetFragmentShader().u_color_transform_matrix(), 1, GL_FALSE,
        color_transform_));
    SetupFragmentShaderAndDraw(graphics_state, program->GetFragmentShader());
  }
}

base::TypeId DrawRectTexture::GetTypeId() const {
  return textures_[1] == NULL
             ? ShaderProgram<ShaderVertexTexcoord,
                             ShaderFragmentTexcoord>::GetTypeId()
             : ShaderProgram<ShaderVertexTexcoord,
                             ShaderFragmentTexcoordYuv3>::GetTypeId();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
