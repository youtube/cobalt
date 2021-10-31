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

#include "cobalt/renderer/rasterizer/egl/draw_rrect_color_texture.h"

#include "base/basictypes.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
const int kVertexCount = 4 * 6;
}  // namespace

DrawRRectColorTexture::DrawRRectColorTexture(
    GraphicsState* graphics_state, const BaseState& base_state,
    const math::RectF& rect, const render_tree::ColorRGBA& color,
    const backend::TextureEGL* texture,
    const math::Matrix3F& texcoord_transform, bool clamp_texcoords)
    : DrawObject(base_state),
      texcoord_transform_(texcoord_transform),
      color_transform_{},
      rect_(rect),
      color_(GetDrawColor(color) * base_state_.opacity),
      textures_{texture, NULL, NULL},
      vertex_buffer_(NULL),
      clamp_texcoords_(clamp_texcoords),
      tile_texture_(false) {
  DCHECK(base_state_.rounded_scissor_corners);
  DCHECK(textures_[0]);
  graphics_state->ReserveVertexData(kVertexCount * sizeof(VertexAttributes));

  // Extract scale from the transform and move it into the vertex attributes
  // so that the anti-aliased edges remain 1 pixel wide.
  math::Vector2dF scale = RemoveScaleFromTransform();
  rect_.Scale(scale.x(), scale.y());
  base_state_.rounded_scissor_rect.Scale(scale.x(), scale.y());
  base_state_.rounded_scissor_corners =
      base_state_.rounded_scissor_corners->Scale(scale.x(), scale.y());
}

DrawRRectColorTexture::DrawRRectColorTexture(
    GraphicsState* graphics_state, const BaseState& base_state,
    const math::RectF& rect, const render_tree::ColorRGBA& color,
    const backend::TextureEGL* y_texture, const backend::TextureEGL* u_texture,
    const backend::TextureEGL* v_texture,
    const float (&color_transform_in_column_major)[16],
    const math::Matrix3F& texcoord_transform, bool clamp_texcoords)
    : DrawObject(base_state),
      texcoord_transform_(texcoord_transform),
      rect_(rect),
      color_(GetDrawColor(color) * base_state_.opacity),
      textures_{y_texture, u_texture, v_texture},
      vertex_buffer_(NULL),
      clamp_texcoords_(clamp_texcoords),
      tile_texture_(false) {
  DCHECK(base_state_.rounded_scissor_corners);
  DCHECK(textures_[0]);
  DCHECK(textures_[1]);
  DCHECK(textures_[2]);
  static_assert(
      sizeof(color_transform_) == sizeof(color_transform_in_column_major),
      "color_transform_ and color_transform_in_column_major size mismatch");

  memcpy(color_transform_, color_transform_in_column_major,
               sizeof(color_transform_));
  graphics_state->ReserveVertexData(kVertexCount * sizeof(VertexAttributes));

  // Extract scale from the transform and move it into the vertex attributes
  // so that the anti-aliased edges remain 1 pixel wide.
  math::Vector2dF scale = RemoveScaleFromTransform();
  rect_.Scale(scale.x(), scale.y());
  base_state_.rounded_scissor_rect.Scale(scale.x(), scale.y());
  base_state_.rounded_scissor_corners =
      base_state_.rounded_scissor_corners->Scale(scale.x(), scale.y());
}

void DrawRRectColorTexture::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  const float kWidthScale = 1.0f / rect_.width();
  const float kHeightScale = 1.0f / rect_.height();

  VertexAttributes attributes[kVertexCount];
  RRectAttributes rrect[4];
  GetRRectAttributes(rect_, base_state_.rounded_scissor_rect,
                     *base_state_.rounded_scissor_corners, rrect);
  for (int r = 0, v = 0; r < arraysize(rrect); ++r) {
    attributes[v].position[0] = rrect[r].bounds.x();
    attributes[v].position[1] = rrect[r].bounds.y();
    attributes[v].rcorner = RCorner(attributes[v].position, rrect[r].rcorner);
    attributes[v + 1].position[0] = rrect[r].bounds.right();
    attributes[v + 1].position[1] = rrect[r].bounds.y();
    attributes[v + 1].rcorner =
        RCorner(attributes[v + 1].position, rrect[r].rcorner);
    attributes[v + 2].position[0] = rrect[r].bounds.x();
    attributes[v + 2].position[1] = rrect[r].bounds.bottom();
    attributes[v + 2].rcorner =
        RCorner(attributes[v + 2].position, rrect[r].rcorner);
    attributes[v + 3].position[0] = rrect[r].bounds.right();
    attributes[v + 3].position[1] = rrect[r].bounds.bottom();
    attributes[v + 3].rcorner =
        RCorner(attributes[v + 3].position, rrect[r].rcorner);

    for (int t = v; t < v + 4; ++t) {
      math::PointF texcoord(
          (attributes[t].position[0] - rect_.x()) * kWidthScale,
          (attributes[t].position[1] - rect_.y()) * kHeightScale);
      texcoord = texcoord_transform_ * texcoord;
      attributes[t].texcoord[0] = texcoord.x();
      attributes[t].texcoord[1] = texcoord.y();
    }

    attributes[v + 4] = attributes[v + 1];
    attributes[v + 5] = attributes[v + 2];
    v += 6;
  }

  vertex_buffer_ = graphics_state->AllocateVertexData(sizeof(attributes));
  memcpy(vertex_buffer_, attributes, sizeof(attributes));

  // Find minimum and maximum texcoord values.
  texcoord_clamps_[0][0] = attributes[0].texcoord[0];
  texcoord_clamps_[0][1] = attributes[0].texcoord[1];
  texcoord_clamps_[0][2] = attributes[0].texcoord[0];
  texcoord_clamps_[0][3] = attributes[0].texcoord[1];
  for (int i = 1; i < arraysize(attributes); ++i) {
    float texcoord_u = attributes[i].texcoord[0];
    float texcoord_v = attributes[i].texcoord[1];
    if (texcoord_clamps_[0][0] > texcoord_u) {
      texcoord_clamps_[0][0] = texcoord_u;
    } else if (texcoord_clamps_[0][2] < texcoord_u) {
      texcoord_clamps_[0][2] = texcoord_u;
    }
    if (texcoord_clamps_[0][1] > texcoord_v) {
      texcoord_clamps_[0][1] = texcoord_v;
    } else if (texcoord_clamps_[0][3] < texcoord_v) {
      texcoord_clamps_[0][3] = texcoord_v;
    }
  }

  tile_texture_ =
      texcoord_clamps_[0][0] < 0.0f || texcoord_clamps_[0][1] < 0.0f ||
      texcoord_clamps_[0][2] > 1.0f || texcoord_clamps_[0][3] > 1.0f;

  for (int i = 1; i < SB_ARRAY_SIZE_INT(texcoord_clamps_); ++i) {
    memcpy(texcoord_clamps_[i], texcoord_clamps_[0],
                 sizeof(texcoord_clamps_[0]));
  }
  if (clamp_texcoords_) {
    // Inset 0.5-epsilon so the border texels are still sampled, but nothing
    // beyond.
    const float kTexelInset = 0.499f;
    for (int i = 0; i < SB_ARRAY_SIZE_INT(texcoord_clamps_); ++i) {
      if (textures_[i] == NULL) {
        break;
      }
      texcoord_clamps_[i][0] += kTexelInset / textures_[i]->GetSize().width();
      texcoord_clamps_[i][1] += kTexelInset / textures_[i]->GetSize().height();
      texcoord_clamps_[i][2] -= kTexelInset / textures_[i]->GetSize().width();
      texcoord_clamps_[i][3] -= kTexelInset / textures_[i]->GetSize().height();
    }
  }
}

void DrawRRectColorTexture::SetupVertexShader(
    GraphicsState* graphics_state,
    const ShaderVertexRcornerTexcoord& vertex_shader) {
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
      vertex_shader.a_rcorner(), 4, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes),
      vertex_buffer_ + offsetof(VertexAttributes, rcorner));
  graphics_state->VertexAttribPointer(
      vertex_shader.a_texcoord(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes),
      vertex_buffer_ + offsetof(VertexAttributes, texcoord));
  graphics_state->VertexAttribFinish();
}

template <typename FragmentShader>
void DrawRRectColorTexture::SetupFragmentShaderAndDraw(
    GraphicsState* graphics_state, const FragmentShader& fragment_shader) {
  GL_CALL(glUniform4f(fragment_shader.u_color(), color_.r(), color_.g(),
                      color_.b(), color_.a()));

  for (int i = 0; i < SB_ARRAY_SIZE_INT(textures_); ++i) {
    if (textures_[i] == NULL) {
      break;
    }

    GL_CALL(glUniform4fv(fragment_shader.u_texcoord_clamp(i), 1,
                         texcoord_clamps_[i]));
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

  GL_CALL(glDrawArrays(GL_TRIANGLES, 0, kVertexCount));

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

void DrawRRectColorTexture::ExecuteRasterize(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  if (textures_[1] == NULL) {
    ShaderProgram<ShaderVertexRcornerTexcoord,
                  ShaderFragmentRcornerTexcoordColor>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupVertexShader(graphics_state, program->GetVertexShader());
    SetupFragmentShaderAndDraw(graphics_state, program->GetFragmentShader());
  } else {
    ShaderProgram<ShaderVertexRcornerTexcoord,
                  ShaderFragmentRcornerTexcoordColorYuv3>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupVertexShader(graphics_state, program->GetVertexShader());
    GL_CALL(glUniformMatrix4fv(
        program->GetFragmentShader().u_color_transform_matrix(), 1, GL_FALSE,
        color_transform_));
    SetupFragmentShaderAndDraw(graphics_state, program->GetFragmentShader());
  }
}

base::TypeId DrawRRectColorTexture::GetTypeId() const {
  return textures_[1] == NULL
             ? ShaderProgram<ShaderVertexRcornerTexcoord,
                             ShaderFragmentRcornerTexcoordColor>::GetTypeId()
             : ShaderProgram<
                   ShaderVertexRcornerTexcoord,
                   ShaderFragmentRcornerTexcoordColorYuv3>::GetTypeId();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
