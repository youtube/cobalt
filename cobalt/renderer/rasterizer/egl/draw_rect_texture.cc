/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/egl/draw_rect_texture.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
struct VertexAttributes {
  float position[3];
  float texcoord[2];
};
}  // namespace

GLuint DrawRectTexture::vertex_shader_ = 0;
GLuint DrawRectTexture::fragment_shader_ = 0;
GLuint DrawRectTexture::program_ = 0;
GLint DrawRectTexture::u_clip_adjustment_ = -1;
GLint DrawRectTexture::u_view_matrix_ = -1;
GLint DrawRectTexture::a_position_ = -1;
GLint DrawRectTexture::a_texcoord_ = -1;
GLint DrawRectTexture::u_texture0_ = -1;

DrawRectTexture::DrawRectTexture(GraphicsState* graphics_state,
    const BaseState& base_state,
    const math::RectF& rect, const backend::TextureEGL* texture,
    const math::Matrix3F& texcoord_transform)
    : DrawObject(base_state),
      texcoord_transform_(texcoord_transform),
      rect_(rect),
      texture_(texture),
      vertex_buffer_(NULL) {
  graphics_state->ReserveVertexData(4 * sizeof(VertexAttributes));
}

void DrawRectTexture::Execute(GraphicsState* graphics_state,
                              ExecutionStage stage) {
  if (stage == kStageUpdateVertexBuffer) {
    VertexAttributes attributes[4] = {
      { { rect_.x(), rect_.bottom(), base_state_.depth },      // uv = (0,1)
        { texcoord_transform_(0, 1) + texcoord_transform_(0, 2),
          texcoord_transform_(1, 1) + texcoord_transform_(1, 2) } },
      { { rect_.right(), rect_.bottom(), base_state_.depth },  // uv = (1,1)
        { texcoord_transform_(0, 0) + texcoord_transform_(0, 1) +
            texcoord_transform_(0, 2),
          texcoord_transform_(1, 0) + texcoord_transform_(1, 1) +
            texcoord_transform_(1, 2) } },
      { { rect_.right(), rect_.y(), base_state_.depth },       // uv = (1,0)
        { texcoord_transform_(0, 0) + texcoord_transform_(0, 2),
          texcoord_transform_(1, 0) + texcoord_transform_(1, 2) } },
      { { rect_.x(), rect_.y(), base_state_.depth },           // uv = (0,0)
        { texcoord_transform_(0, 2), texcoord_transform_(1, 2) } },
    };
    COMPILE_ASSERT(sizeof(attributes) == 4 * sizeof(VertexAttributes),
                   bad_padding);
    vertex_buffer_ = graphics_state->AllocateVertexData(
        sizeof(attributes));
    memcpy(vertex_buffer_, attributes, sizeof(attributes));
  } else if (stage == kStageRasterizeNormal) {
    graphics_state->UseProgram(program_);
    graphics_state->UpdateClipAdjustment(u_clip_adjustment_);
    graphics_state->UpdateTransformMatrix(u_view_matrix_,
        base_state_.transform);
    graphics_state->Scissor(base_state_.scissor.x(), base_state_.scissor.y(),
        base_state_.scissor.width(), base_state_.scissor.height());
    graphics_state->VertexAttribPointer(a_position_, 3, GL_FLOAT, GL_FALSE,
        sizeof(VertexAttributes), vertex_buffer_);
    graphics_state->VertexAttribPointer(a_texcoord_, 2, GL_FLOAT, GL_FALSE,
        sizeof(VertexAttributes), vertex_buffer_ + 3*sizeof(float));
    graphics_state->VertexAttribFinish();
    graphics_state->DisableBlend();
    graphics_state->ActiveTexture(GL_TEXTURE0);
    GL_CALL(glBindTexture(texture_->GetTarget(), texture_->gl_handle()));
    GL_CALL(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
  }
}

// static
void DrawRectTexture::CreateResources() {
  const char kVertexSource[] =
      "uniform vec4 u_clip_adjustment;"
      "uniform mat3 u_view_matrix;"
      "attribute vec3 a_position;"
      "attribute vec2 a_texcoord;"
      "varying vec2 v_texcoord;"
      "void main() {"
      "  vec3 pos2d = u_view_matrix * vec3(a_position.xy, 1);"
      "  gl_Position = vec4(pos2d.xy * u_clip_adjustment.xy +"
      "                     u_clip_adjustment.zw, a_position.z, pos2d.z);"
      "  v_texcoord = a_texcoord;"
      "}";
  const char kFragmentSource[] =
      "precision mediump float;"
      "uniform sampler2D u_texture0;"
      "varying vec2 v_texcoord;"
      "void main() {"
      "  gl_FragColor = texture2D(u_texture0, v_texcoord);"
      "}";

  program_ = glCreateProgram();

  vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
  const GLchar* vertex_source = kVertexSource;
  GLint vertex_length = static_cast<GLint>(sizeof(kVertexSource) - 1);
  GL_CALL(glShaderSource(vertex_shader_, 1, &vertex_source,
                         &vertex_length));
  GL_CALL(glCompileShader(vertex_shader_));
  GL_CALL(glAttachShader(program_, vertex_shader_));

  fragment_shader_ = glCreateShader(GL_FRAGMENT_SHADER);
  const GLchar* fragment_source = kFragmentSource;
  GLint fragment_length = static_cast<GLint>(sizeof(kFragmentSource) - 1);
  GL_CALL(glShaderSource(fragment_shader_, 1, &fragment_source,
                         &fragment_length));
  GL_CALL(glCompileShader(fragment_shader_));
  GL_CALL(glAttachShader(program_, fragment_shader_));

  GL_CALL(glLinkProgram(program_));

  u_clip_adjustment_ = glGetUniformLocation(program_, "u_clip_adjustment");
  DCHECK_NE(u_clip_adjustment_, -1);
  u_view_matrix_ = glGetUniformLocation(program_, "u_view_matrix");
  DCHECK_NE(u_view_matrix_, -1);
  a_position_ = glGetAttribLocation(program_, "a_position");
  DCHECK_NE(a_position_, -1);
  a_texcoord_ = glGetAttribLocation(program_, "a_texcoord");
  DCHECK_NE(a_texcoord_, -1);
  u_texture0_ = glGetUniformLocation(program_, "u_texture0");
  DCHECK_NE(u_texture0_, -1);
  GL_CALL(glUseProgram(program_));
  GL_CALL(glUniform1i(u_texture0_, 0));
  GL_CALL(glUseProgram(0));
}

// static
void DrawRectTexture::DestroyResources() {
  GL_CALL(glFinish());
  GL_CALL(glDeleteProgram(program_));
  GL_CALL(glDeleteShader(fragment_shader_));
  GL_CALL(glDeleteShader(vertex_shader_));
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
