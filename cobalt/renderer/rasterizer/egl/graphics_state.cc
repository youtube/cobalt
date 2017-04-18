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
    : frame_index_(0),
      vertex_data_reserved_(kVertexDataAlignment - 1),
      vertex_data_allocated_(0),
      vertex_data_buffer_updated_(false) {
  GL_CALL(glGenBuffers(kNumFramesBuffered, &vertex_data_buffer_handle_[0]));
  memset(clip_adjustment_, 0, sizeof(clip_adjustment_));
  SetDirty();
  blend_enabled_ = false;
  depth_test_enabled_ = false;
  depth_write_enabled_ = true;
  Reset();

  // These settings should only need to be set once. Nothing should touch them.
  GL_CALL(glDepthRangef(0.0f, 1.0f));
  GL_CALL(glDisable(GL_DITHER));
  GL_CALL(glDisable(GL_CULL_FACE));
  GL_CALL(glDisable(GL_STENCIL_TEST));
}

GraphicsState::~GraphicsState() {
  GL_CALL(glDeleteBuffers(kNumFramesBuffered, &vertex_data_buffer_handle_[0]));
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

  // Reset to default GL state. Assume the current state is dirty, so just
  // set the cached state. The actual GL state will be updated to match the
  // cached state when needed.
  SetDirty();
  blend_enabled_ = false;
  depth_test_enabled_ = false;
  depth_write_enabled_ = true;
}

void GraphicsState::EndFrame() {
  // Reset the vertex data buffer.
  vertex_data_reserved_ = kVertexDataAlignment - 1;
  vertex_data_allocated_ = 0;
  vertex_data_buffer_updated_ = false;
  frame_index_ = (frame_index_ + 1) % kNumFramesBuffered;

  // Force default GL state. The current state may be marked dirty, so don't
  // rely on any functions which check the cached state before issuing GL calls.
  GL_CALL(glDisable(GL_BLEND));
  GL_CALL(glDisable(GL_DEPTH_TEST));
  GL_CALL(glDepthMask(GL_TRUE));
  GL_CALL(glUseProgram(0));

  // Since the GL state was changed without going through the cache, mark it
  // as dirty.
  SetDirty();
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

  if (array_buffer_handle_ != vertex_data_buffer_handle_[frame_index_]) {
    array_buffer_handle_ = vertex_data_buffer_handle_[frame_index_];
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer_handle_));
  }

  // Disable any vertex attribute arrays that will not be used.
  disable_vertex_attrib_array_mask_ = enabled_vertex_attrib_array_mask_;
}

void GraphicsState::Viewport(int x, int y, int width, int height) {
  // Incoming origin is top-left, but GL origin is bottom-left, so flip
  // vertically.
  if (state_dirty_ || viewport_.x() != x || viewport_.y() != y ||
      viewport_.width() != width || viewport_.height() != height) {
    viewport_.SetRect(x, y, width, height);
    GL_CALL(glViewport(x, render_target_size_.height() - y - height,
                       width, height));
  }
}

void GraphicsState::Scissor(int x, int y, int width, int height) {
  // Incoming origin is top-left, but GL origin is bottom-left, so flip
  // vertically.
  if (state_dirty_ || scissor_.x() != x || scissor_.y() != y ||
      scissor_.width() != width || scissor_.height() != height) {
    scissor_.SetRect(x, y, width, height);
    GL_CALL(glScissor(x, render_target_size_.height() - y - height,
                      width, height));
  }
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

void GraphicsState::ResetDepthFunc() {
  GL_CALL(glDepthFunc(GL_LESS));
}

void GraphicsState::ActiveBindTexture(GLenum texture_unit, GLenum target,
                                      GLuint texture) {
  int texunit_index = texture_unit - GL_TEXTURE0;

  // Update only if it doesn't match the current state.
  if (texunit_index >= kNumTextureUnitsCached ||
      texunit_target_[texunit_index] != target ||
      texunit_texture_[texunit_index] != texture) {
    if (texture_unit_ != texture_unit) {
      texture_unit_ = texture_unit;
      GL_CALL(glActiveTexture(texture_unit));
    }
    GL_CALL(glBindTexture(target, texture));

    if (texunit_index < kNumTextureUnitsCached) {
      texunit_target_[texunit_index] = target;
      texunit_texture_[texunit_index] = texture;
    }
  }
}

void GraphicsState::ActiveBindTexture(GLenum texture_unit, GLenum target,
    GLuint texture, GLint texture_wrap_mode) {
  int texunit_index = texture_unit - GL_TEXTURE0;

  if (texture_unit_ != texture_unit) {
    texture_unit_ = texture_unit;
    GL_CALL(glActiveTexture(texture_unit));
    GL_CALL(glBindTexture(target, texture));
  } else if (texunit_index >= kNumTextureUnitsCached ||
             texunit_target_[texunit_index] != target ||
             texunit_texture_[texunit_index] != texture) {
    GL_CALL(glBindTexture(target, texture));
  }

  if (texunit_index < kNumTextureUnitsCached) {
    texunit_target_[texunit_index] = target;
    texunit_texture_[texunit_index] = texture;
  }

  GL_CALL(glTexParameteri(target, GL_TEXTURE_WRAP_S, texture_wrap_mode));
  GL_CALL(glTexParameteri(target, GL_TEXTURE_WRAP_T, texture_wrap_mode));
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
  if (array_buffer_handle_ != vertex_data_buffer_handle_[frame_index_]) {
    array_buffer_handle_ = vertex_data_buffer_handle_[frame_index_];
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer_handle_));
  }
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
  enabled_vertex_attrib_array_mask_ &= ~disable_vertex_attrib_array_mask_;
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

  Viewport(viewport_.x(), viewport_.y(), viewport_.width(), viewport_.height());
  Scissor(scissor_.x(), scissor_.y(), scissor_.width(), scissor_.height());
  GL_CALL(glEnable(GL_SCISSOR_TEST));

  array_buffer_handle_ = 0;
  texture_unit_ = 0;
  memset(&texunit_target_, 0, sizeof(texunit_target_));
  memset(&texunit_texture_, 0, sizeof(texunit_texture_));
  enabled_vertex_attrib_array_mask_ = 0;
  disable_vertex_attrib_array_mask_ = 0;
  clip_adjustment_dirty_ = true;

  if (vertex_data_buffer_updated_) {
    array_buffer_handle_ = vertex_data_buffer_handle_[frame_index_];
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer_handle_));
  }

  if (blend_enabled_) {
    GL_CALL(glEnable(GL_BLEND));
  } else {
    GL_CALL(glDisable(GL_BLEND));
  }
  GL_CALL(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));

  if (depth_test_enabled_) {
    GL_CALL(glEnable(GL_DEPTH_TEST));
  } else {
    GL_CALL(glDisable(GL_DEPTH_TEST));
  }
  GL_CALL(glDepthMask(depth_write_enabled_ ? GL_TRUE : GL_FALSE));
  ResetDepthFunc();

  state_dirty_ = false;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
