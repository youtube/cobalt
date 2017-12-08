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

namespace {

void GLViewport(const math::Rect& viewport, const math::Size& target_size) {
  // Incoming origin is top-left, but GL origin is bottom-left, so flip
  // vertically.
  GL_CALL(glViewport(viewport.x(), target_size.height() - viewport.bottom(),
                     viewport.width(), viewport.height()));
}

void GLScissor(const math::Rect& scissor, const math::Size& target_size) {
  // Incoming origin is top-left, but GL origin is bottom-left, so flip
  // vertically.
  GL_CALL(glScissor(scissor.x(), target_size.height() - scissor.bottom(),
                    scissor.width(), scissor.height()));
}

}  // namespace

GraphicsState::GraphicsState()
    : render_target_handle_(0),
      render_target_serial_(0),
      frame_index_(0),
      vertex_data_capacity_(0),
      vertex_data_reserved_(kVertexDataAlignment - 1),
      vertex_data_allocated_(0),
      vertex_index_capacity_(0),
      vertex_index_reserved_(0),
      vertex_index_allocated_(0),
      vertex_buffers_updated_(false) {
  // https://www.khronos.org/registry/OpenGL-Refpages/es2.0/xhtml/glGet.xml
  // GL_MAX_VERTEX_ATTRIBS should return at least 8. So default to that in
  // case the glGetIntergerv call fails.
  max_vertex_attribs_ = 8;
  GL_CALL(glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs_));

  GL_CALL(glGenBuffers(kNumFramesBuffered, vertex_data_buffer_handle_));
  GL_CALL(glGenBuffers(kNumFramesBuffered, vertex_index_buffer_handle_));
  memset(clip_adjustment_, 0, sizeof(clip_adjustment_));
  SetDirty();
  blend_enabled_ = false;
  Reset();
}

GraphicsState::~GraphicsState() {
  GL_CALL(glDeleteBuffers(kNumFramesBuffered, vertex_data_buffer_handle_));
  GL_CALL(glDeleteBuffers(kNumFramesBuffered, vertex_index_buffer_handle_));
}

void GraphicsState::SetDirty() {
  state_dirty_ = true;
  clip_adjustment_dirty_ = true;
}

void GraphicsState::BeginFrame() {
  DCHECK_EQ(vertex_data_allocated_, 0);
  if (vertex_data_reserved_ > vertex_data_capacity_) {
    vertex_data_capacity_ = vertex_data_reserved_;
    vertex_data_buffer_.reset(new uint8_t[vertex_data_capacity_]);
  }
  DCHECK_EQ(vertex_index_allocated_, 0);
  if (vertex_index_reserved_ > vertex_index_capacity_) {
    vertex_index_capacity_ = vertex_index_reserved_;
    vertex_index_buffer_.reset(new uint16_t[vertex_index_capacity_]);
  }

  // Reset to default GL state. Assume the current state is dirty, so just
  // set the cached state. The actual GL state will be updated to match the
  // cached state when needed.
  SetDirty();
  blend_enabled_ = false;
}

void GraphicsState::EndFrame() {
  // Reset the vertex data and index buffers.
  vertex_data_reserved_ = kVertexDataAlignment - 1;
  vertex_data_allocated_ = 0;
  vertex_index_reserved_ = 0;
  vertex_index_allocated_ = 0;
  vertex_buffers_updated_ = false;
  frame_index_ = (frame_index_ + 1) % kNumFramesBuffered;

  // Force default GL state. The current state may be marked dirty, so don't
  // rely on any functions which check the cached state before issuing GL calls.
  GL_CALL(glDisable(GL_BLEND));
  GL_CALL(glUseProgram(0));

  // Since the GL state was changed without going through the cache, mark it
  // as dirty.
  SetDirty();
}

void GraphicsState::Clear() {
  Clear(0.0f, 0.0f, 0.0f, 0.0f);
}

void GraphicsState::Clear(float r, float g, float b, float a) {
  if (state_dirty_) {
    Reset();
  }

  GL_CALL(glClearColor(r, g, b, a));
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
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

  if (vertex_data_allocated_ > 0 &&
      array_buffer_handle_ != vertex_data_buffer_handle_[frame_index_]) {
    array_buffer_handle_ = vertex_data_buffer_handle_[frame_index_];
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer_handle_));
  }
  if (vertex_index_allocated_ > 0 &&
      index_buffer_handle_ != vertex_index_buffer_handle_[frame_index_]) {
    index_buffer_handle_ = vertex_index_buffer_handle_[frame_index_];
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_handle_));
  }

  // Disable any vertex attribute arrays that will not be used.
  disable_vertex_attrib_array_mask_ = enabled_vertex_attrib_array_mask_;
}

void GraphicsState::BindFramebuffer(
    const backend::RenderTarget* render_target) {
  if (render_target == nullptr) {
    // Unbind the framebuffer immediately.
    if (render_target_handle_ != 0) {
      GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }
    render_target_handle_ = 0;
    render_target_serial_ = 0;
    render_target_size_.SetSize(0, 0);
    SetClipAdjustment();
  } else if (render_target->GetPlatformHandle() != render_target_handle_ ||
      render_target->GetSerialNumber() != render_target_serial_ ||
      render_target->GetSize() != render_target_size_) {
    render_target_handle_ = render_target->GetPlatformHandle();
    render_target_serial_ = render_target->GetSerialNumber();
    render_target_size_ = render_target->GetSize();
    SetClipAdjustment();

    if (!state_dirty_) {
      GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, render_target_handle_));
    }
  }
}

void GraphicsState::Viewport(int x, int y, int width, int height) {
  if (viewport_.x() != x || viewport_.y() != y ||
      viewport_.width() != width || viewport_.height() != height) {
    viewport_.SetRect(x, y, width, height);
    if (!state_dirty_) {
      GLViewport(viewport_, render_target_size_);
    }
  }
}

void GraphicsState::Scissor(int x, int y, int width, int height) {
  if (scissor_.x() != x || scissor_.y() != y ||
      scissor_.width() != width || scissor_.height() != height) {
    scissor_.SetRect(x, y, width, height);
    if (!state_dirty_) {
      GLScissor(scissor_, render_target_size_);
    }
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

void GraphicsState::SetClipAdjustment() {
  clip_adjustment_dirty_ = true;

  // Clip adjustment is a vec4 used to transform a given 2D position from view
  // space to clip space. Given a 2D position, pos, the output is:
  // output = pos * clip_adjustment_.xy + clip_adjustment_.zw

  if (render_target_size_.width() > 0) {
    clip_adjustment_[0] = 2.0f / render_target_size_.width();
    clip_adjustment_[2] = -1.0f;
  } else {
    clip_adjustment_[0] = 0.0f;
    clip_adjustment_[2] = 0.0f;
  }

  if (render_target_size_.height() > 0) {
    // Incoming origin is top-left, but GL origin is bottom-left, so flip the
    // image vertically.
    clip_adjustment_[1] = -2.0f / render_target_size_.height();
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
  DCHECK(!vertex_buffers_updated_);
  vertex_data_reserved_ += bytes + (kVertexDataAlignment - 1) &
                           ~(kVertexDataAlignment - 1);
}

uint8_t* GraphicsState::AllocateVertexData(size_t bytes) {
  DCHECK(!vertex_buffers_updated_);

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

void GraphicsState::ReserveVertexIndices(size_t count) {
  DCHECK_EQ(vertex_index_allocated_, 0);
  DCHECK(!vertex_buffers_updated_);
  vertex_index_reserved_ += count;
}

uint16_t* GraphicsState::AllocateVertexIndices(size_t count) {
  uint16_t* client_pointer = &vertex_index_buffer_[vertex_index_allocated_];
  vertex_index_allocated_ += count;
  DCHECK_LE(vertex_index_allocated_, vertex_index_reserved_);
  return client_pointer;
}

const void* GraphicsState::GetVertexIndexPointer(
    const uint16_t* client_pointer) {
  DCHECK_GE(client_pointer, &vertex_index_buffer_[0]);
  DCHECK_LE(client_pointer, &vertex_index_buffer_[vertex_index_allocated_]);
  const GLvoid* gl_pointer = reinterpret_cast<const GLvoid*>(
      reinterpret_cast<const uint8_t*>(client_pointer) -
      reinterpret_cast<const uint8_t*>(&vertex_index_buffer_[0]));
  return gl_pointer;
}

void GraphicsState::UpdateVertexBuffers() {
  DCHECK(!vertex_buffers_updated_);
  vertex_buffers_updated_ = true;

  if (vertex_data_allocated_ > 0) {
    if (array_buffer_handle_ != vertex_data_buffer_handle_[frame_index_]) {
      array_buffer_handle_ = vertex_data_buffer_handle_[frame_index_];
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer_handle_));
    }
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, vertex_data_allocated_,
                         &vertex_data_buffer_[0], GL_DYNAMIC_DRAW));
  }

  if (vertex_index_allocated_ > 0) {
    if (index_buffer_handle_ != vertex_index_buffer_handle_[frame_index_]) {
      index_buffer_handle_ = vertex_index_buffer_handle_[frame_index_];
      GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_handle_));
    }
    GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         vertex_index_allocated_ * sizeof(uint16_t),
                         &vertex_index_buffer_[0], GL_DYNAMIC_DRAW));
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

void GraphicsState::Reset() {
  program_ = 0;

  GL_CALL(glDisable(GL_DITHER));
  GL_CALL(glDisable(GL_STENCIL_TEST));
  GL_CALL(glDisable(GL_CULL_FACE));

  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, render_target_handle_));
  GLViewport(viewport_, render_target_size_);
  GLScissor(scissor_, render_target_size_);
  GL_CALL(glEnable(GL_SCISSOR_TEST));

  array_buffer_handle_ = 0;
  index_buffer_handle_ = 0;
  texture_unit_ = 0;
  memset(&texunit_target_, 0, sizeof(texunit_target_));
  memset(&texunit_texture_, 0, sizeof(texunit_texture_));
  clip_adjustment_dirty_ = true;

  enabled_vertex_attrib_array_mask_ = 0;
  disable_vertex_attrib_array_mask_ = 0;
  for (int index = 0; index < max_vertex_attribs_; ++index) {
    GL_CALL(glDisableVertexAttribArray(index));
  }

  if (vertex_buffers_updated_) {
    if (vertex_data_allocated_ > 0) {
      array_buffer_handle_ = vertex_data_buffer_handle_[frame_index_];
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, array_buffer_handle_));
    }
    if (vertex_index_allocated_ > 0) {
      index_buffer_handle_ = vertex_index_buffer_handle_[frame_index_];
      GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_handle_));
    }
  }

  if (blend_enabled_) {
    GL_CALL(glEnable(GL_BLEND));
  } else {
    GL_CALL(glDisable(GL_BLEND));
  }
  GL_CALL(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));

  GL_CALL(glDisable(GL_DEPTH_TEST));

  state_dirty_ = false;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
