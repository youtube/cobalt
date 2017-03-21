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

#include "cobalt/renderer/rasterizer/egl/graphics_state.h"

#include <algorithm>

#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

GraphicsState::GraphicsState()
    : vertex_data_reserved_(kVertexDataAlignment - 1),
      vertex_data_allocated_(0),
      vertex_data_buffer_handle_(0),
      vertex_data_buffer_updated_(false) {
  GL_CALL(glGenBuffers(1, &vertex_data_buffer_handle_));
  memset(clip_adjustment_, 0, sizeof(clip_adjustment_));
  Reset();
}

GraphicsState::~GraphicsState() {
  if (vertex_data_buffer_handle_ != 0) {
    GL_CALL(glDeleteBuffers(1, &vertex_data_buffer_handle_));
  }
}

void GraphicsState::SetDirty() {
  state_dirty_ = true;
  clip_adjustment_dirty_ = true;
}

void GraphicsState::BeginFrame() {
  DCHECK_EQ(vertex_data_allocated_, 0);
  if (vertex_data_reserved_ > vertex_data_buffer_.capacity()) {
    vertex_data_buffer_.reserve(vertex_data_reserved_);
  }
  SetDirty();
}

void GraphicsState::EndFrame() {
  // Reset the vertex data buffer.
  vertex_data_reserved_ = kVertexDataAlignment - 1;
  vertex_data_allocated_ = 0;
  vertex_data_buffer_updated_ = false;
  UseProgram(0);
}

void GraphicsState::Clear() {
  if (state_dirty_) {
    Reset();
  }

  GL_CALL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
  GL_CALL(glClearDepthf(FarthestDepth()));
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

void GraphicsState::UseProgram(GLuint program) {
  if (state_dirty_) {
    Reset();
  }

  if (program_ != program) {
    program_ = program;
    clip_adjustment_dirty_ = true;
    GL_CALL(glUseProgram(program));
  }

  if (array_buffer_handle_ != vertex_data_buffer_handle_) {
    array_buffer_handle_ = vertex_data_buffer_handle_;
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vertex_data_buffer_handle_));
  }

  // Disable any vertex attribute arrays that will not be used.
  disable_vertex_attrib_array_mask_ = enabled_vertex_attrib_array_mask_;
}

void GraphicsState::Viewport(int x, int y, int width, int height) {
  viewport_ = math::Rect(x, y, width, height);
  // Incoming origin is top-left, but GL origin is bottom-left, so flip
  // vertically.
  GL_CALL(glViewport(x, render_target_size_.height() - y - height,
                     width, height));
}

void GraphicsState::Scissor(int x, int y, int width, int height) {
  // Incoming origin is top-left, but GL origin is bottom-left, so flip
  // vertically.
  scissor_ = math::Rect(x, y, width, height);
  GL_CALL(glScissor(x, render_target_size_.height() - y - height,
                    width, height));
}

void GraphicsState::EnableBlend() {
  if (!blend_enabled_) {
    blend_enabled_ = true;
    GL_CALL(glEnable(GL_BLEND));
  }
}

void GraphicsState::DisableBlend() {
  if (blend_enabled_) {
    blend_enabled_ = false;
    GL_CALL(glDisable(GL_BLEND));
  }
}

void GraphicsState::EnableDepthTest() {
  if (!depth_test_enabled_) {
    depth_test_enabled_ = true;
    GL_CALL(glEnable(GL_DEPTH_TEST));
  }
}

void GraphicsState::DisableDepthTest() {
  if (depth_test_enabled_) {
    depth_test_enabled_ = false;
    GL_CALL(glDisable(GL_DEPTH_TEST));
  }
}

void GraphicsState::EnableDepthWrite() {
  if (!depth_write_enabled_) {
    depth_write_enabled_ = true;
    GL_CALL(glDepthMask(GL_TRUE));
  }
}

void GraphicsState::DisableDepthWrite() {
  if (depth_write_enabled_) {
    depth_write_enabled_ = false;
    GL_CALL(glDepthMask(GL_FALSE));
  }
}

void GraphicsState::ActiveTexture(GLenum texture_unit) {
  if (texture_unit_ != texture_unit) {
    texture_unit_ = texture_unit;
    GL_CALL(glActiveTexture(texture_unit));
  }
}

void GraphicsState::SetClipAdjustment(const math::Size& render_target_size) {
  render_target_size_ = render_target_size;
  clip_adjustment_dirty_ = true;

  // Clip adjustment is a vec4 used to transform a given 2D position from view
  // space to clip space. Given a 2D position, pos, the output is:
  // output = pos * clip_adjustment_.xy + clip_adjustment_.zw

  if (render_target_size.width() > 0) {
    clip_adjustment_[0] = 2.0f / render_target_size.width();
    clip_adjustment_[2] = -1.0f;
  } else {
    clip_adjustment_[0] = 0.0f;
    clip_adjustment_[2] = 0.0f;
  }

  if (render_target_size.height() > 0) {
    // Incoming origin is top-left, but GL origin is bottom-left, so flip the
    // image vertically.
    clip_adjustment_[1] = -2.0f / render_target_size.height();
    clip_adjustment_[3] = 1.0f;
  } else {
    clip_adjustment_[1] = 0.0f;
    clip_adjustment_[3] = 0.0f;
  }
}

void GraphicsState::UpdateClipAdjustment(GLint handle) {
  if (clip_adjustment_dirty_) {
    clip_adjustment_dirty_ = false;
    GL_CALL(glUniform4fv(handle, 1, clip_adjustment_));
  }
}

void GraphicsState::UpdateTransformMatrix(GLint handle,
                                             const math::Matrix3F& transform) {
  // Manually transpose our row-major matrix to column-major. Don't rely on
  // glUniformMatrix3fv to do it, since the driver may not support that.
  float transpose[] = {
    transform(0, 0), transform(1, 0), transform(2, 0),
    transform(0, 1), transform(1, 1), transform(2, 1),
    transform(0, 2), transform(1, 2), transform(2, 2)
  };
  GL_CALL(glUniformMatrix3fv(handle, 1, GL_FALSE, transpose));
}

void GraphicsState::ReserveVertexData(size_t bytes) {
  DCHECK_EQ(vertex_data_allocated_, 0);
  DCHECK(!vertex_data_buffer_updated_);
  vertex_data_reserved_ += bytes + (kVertexDataAlignment - 1) &
                           ~(kVertexDataAlignment - 1);
}

uint8_t* GraphicsState::AllocateVertexData(size_t bytes) {
  DCHECK(!vertex_data_buffer_updated_);

  // Ensure the start address is aligned.
  uintptr_t start_address =
      reinterpret_cast<uintptr_t>(&vertex_data_buffer_[0]) +
      vertex_data_allocated_ + (kVertexDataAlignment - 1) &
      ~(kVertexDataAlignment - 1);

  vertex_data_allocated_ = start_address -
      reinterpret_cast<uintptr_t>(&vertex_data_buffer_[0]) + bytes;

  DCHECK_LE(vertex_data_allocated_, vertex_data_reserved_);
  return reinterpret_cast<uint8_t*>(start_address);
}

void GraphicsState::UpdateVertexData() {
  DCHECK(!vertex_data_buffer_updated_);
  vertex_data_buffer_updated_ = true;
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vertex_data_buffer_handle_));
  if (vertex_data_allocated_ > 0) {
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, vertex_data_allocated_,
                         &vertex_data_buffer_[0], GL_STREAM_DRAW));
  }
}

void GraphicsState::VertexAttribPointer(GLint index, GLint size, GLenum type,
    GLboolean normalized, GLsizei stride, const void* client_pointer) {
  const GLvoid* gl_pointer = reinterpret_cast<const GLvoid*>
      (reinterpret_cast<const uint8_t*>(client_pointer) -
      &vertex_data_buffer_[0]);
  GL_CALL(glVertexAttribPointer(index, size, type, normalized, stride,
                                gl_pointer));

  // Ensure the vertex attrib array is enabled.
  const uint32_t mask = 1 << index;
  if ((enabled_vertex_attrib_array_mask_ & mask) == 0) {
    enabled_vertex_attrib_array_mask_ |= mask;
    GL_CALL(glEnableVertexAttribArray(index));
  }
  disable_vertex_attrib_array_mask_ &= ~mask;
}

void GraphicsState::VertexAttribFinish() {
  for (int index = 0; disable_vertex_attrib_array_mask_ != 0; ++index) {
    if ((disable_vertex_attrib_array_mask_ & 1) != 0) {
      GL_CALL(glDisableVertexAttribArray(index));
    }
    disable_vertex_attrib_array_mask_ >>= 1;
  }
}

// static
float GraphicsState::FarthestDepth() {
  return 1.0f;
}

// static
float GraphicsState::NextClosestDepth(float depth) {
  // Our vertex shaders pass depth straight to gl_Position without any
  // transformation, and gl_Position.w is always 1. To avoid clipping,
  // |depth| should be [-1,1]. However, this range is then converted to [0,1],
  // then scaled by (2^N - 1) to be the final value in the depth buffer, where
  // N = number of bits in the depth buffer.

  // In the worst case, we're using a 16-bit depth buffer. So each step in
  // depth is 2 / (2^N - 1) (since depth is converted from [-1,1] to [0,1]).
  // Also, because the default depth compare function is GL_LESS, vertices with
  // smaller depth values appear on top of others.
  return depth - 2.0f / 65535.0f;
}

void GraphicsState::Reset() {
  program_ = 0;
  GL_CALL(glUseProgram(0));

  Viewport(viewport_.x(), viewport_.y(), viewport_.width(), viewport_.height());
  Scissor(scissor_.x(), scissor_.y(), scissor_.width(), scissor_.height());

  array_buffer_handle_ = 0;
  texture_unit_ = 0;
  enabled_vertex_attrib_array_mask_ = 0;
  disable_vertex_attrib_array_mask_ = 0;
  clip_adjustment_dirty_ = true;

  blend_enabled_ = true;
  DisableBlend();
  GL_CALL(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));

  depth_test_enabled_ = false;
  EnableDepthTest();
  GL_CALL(glDepthFunc(GL_LESS));
  GL_CALL(glDepthRangef(0.0f, 1.0f));

  depth_write_enabled_ = false;
  EnableDepthWrite();

  GL_CALL(glDisable(GL_DITHER));
  GL_CALL(glDisable(GL_CULL_FACE));
  GL_CALL(glDisable(GL_STENCIL_TEST));
  GL_CALL(glEnable(GL_SCISSOR_TEST));

  state_dirty_ = false;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
