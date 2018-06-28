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
const int kVertexCount = 4 * 6;
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

  // Extract scale from the transform and move it into the vertex attributes
  // so that the anti-aliased edges remain 1 pixel wide.
  math::Vector2dF scale = RemoveScaleFromTransform();
  rect_.Scale(scale.x(), scale.y());
  base_state_.rounded_scissor_rect.Scale(scale.x(), scale.y());
  base_state_.rounded_scissor_corners =
      base_state_.rounded_scissor_corners->Scale(scale.x(), scale.y());
}

void DrawRRectColorTexture::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  const float kWidthScale = 1.0f / rect_.width();
  const float kHeightScale = 1.0f / rect_.height();

  VertexAttributes attributes[kVertexCount];
  RRectAttributes rrect[4];
  GetRRectAttributes(rect_, base_state_.rounded_scissor_rect,
                     *base_state_.rounded_scissor_corners, rrect);
  for (int r = 0, v = 0; r < arraysize(rrect); ++r) {
    attributes[v  ].position[0] = rrect[r].bounds.x();
    attributes[v  ].position[1] = rrect[r].bounds.y();
    attributes[v  ].rcorner =
        RCorner(attributes[v  ].position, rrect[r].rcorner);
    attributes[v+1].position[0] = rrect[r].bounds.right();
    attributes[v+1].position[1] = rrect[r].bounds.y();
    attributes[v+1].rcorner =
        RCorner(attributes[v+1].position, rrect[r].rcorner);
    attributes[v+2].position[0] = rrect[r].bounds.x();
    attributes[v+2].position[1] = rrect[r].bounds.bottom();
    attributes[v+2].rcorner =
        RCorner(attributes[v+2].position, rrect[r].rcorner);
    attributes[v+3].position[0] = rrect[r].bounds.right();
    attributes[v+3].position[1] = rrect[r].bounds.bottom();
    attributes[v+3].rcorner =
        RCorner(attributes[v+3].position, rrect[r].rcorner);

    for (int t = v; t < v + 4; ++t) {
      math::PointF texcoord(
          (attributes[t].position[0] - rect_.x()) * kWidthScale,
          (attributes[t].position[1] - rect_.y()) * kHeightScale);
      texcoord = texcoord_transform_ * texcoord;
      attributes[t].texcoord[0] = texcoord.x();
      attributes[t].texcoord[1] = texcoord.y();
    }

    attributes[v+4] = attributes[v+1];
    attributes[v+5] = attributes[v+2];
    v += 6;
  }

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
  ShaderProgram<ShaderVertexRcornerTexcoord,
                ShaderFragmentRcornerTexcoordColor>* program;
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
      program->GetVertexShader().a_rcorner(), 4, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, rcorner));
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_texcoord(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, texcoord));
  graphics_state->VertexAttribFinish();

  GL_CALL(glUniform4f(program->GetFragmentShader().u_color(),
                      color_.r(), color_.g(), color_.b(), color_.a()));
  GL_CALL(glUniform4fv(program->GetFragmentShader().u_texcoord_clamp(), 1,
                       texcoord_clamp_));

  if (tile_texture_) {
    graphics_state->ActiveBindTexture(
        program->GetFragmentShader().u_texture_texunit(),
        texture_->GetTarget(), texture_->gl_handle(), GL_REPEAT);
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, kVertexCount));
    graphics_state->ActiveBindTexture(
        program->GetFragmentShader().u_texture_texunit(),
        texture_->GetTarget(), texture_->gl_handle(), GL_CLAMP_TO_EDGE);
  } else {
    graphics_state->ActiveBindTexture(
        program->GetFragmentShader().u_texture_texunit(),
        texture_->GetTarget(), texture_->gl_handle());
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, kVertexCount));
  }
}

base::TypeId DrawRRectColorTexture::GetTypeId() const {
  return ShaderProgram<ShaderVertexRcornerTexcoord,
                       ShaderFragmentRcornerTexcoordColor>::GetTypeId();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
