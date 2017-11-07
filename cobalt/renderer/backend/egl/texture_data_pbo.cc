// Copyright 2015 Google Inc. All Rights Reserved.
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

#if defined(GLES3_SUPPORTED)

#include "cobalt/renderer/backend/egl/texture_data_pbo.h"

#include <GLES2/gl2ext.h>

#include "base/bind.h"
#include "base/logging.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace backend {

TextureDataPBO::TextureDataPBO(ResourceContext* resource_context,
                               const math::Size& size, GLenum format)
    : resource_context_(resource_context),
      size_(size),
      format_(format),
      mapped_data_(NULL),
      error_(false) {
  data_size_ = static_cast<int64>(GetPitchInBytes()) * size_.height();

  resource_context_->RunSynchronouslyWithinResourceContext(
      base::Bind(&TextureDataPBO::InitAndMapPBO, base::Unretained(this)));
}

void TextureDataPBO::InitAndMapPBO() {
  resource_context_->AssertWithinResourceContext();

  // Use the resource context to allocate a GL pixel unpack buffer with the
  // specified size.
  glGenBuffers(1, &pixel_buffer_);
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Error creating new buffer.";
    error_ = true;
    return;
  }

  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer_));
  glBufferData(GL_PIXEL_UNPACK_BUFFER, data_size_, 0, GL_STATIC_DRAW);
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Error allocating PBO data for image.";
    error_ = true;
    GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
    GL_CALL(glDeleteBuffers(1, &pixel_buffer_));
    return;
  }

  // Map the GL pixel buffer data to CPU addressable memory.  We pass the flags
  // MAP_INVALIDATE_BUFFER_BIT | MAP_UNSYNCHRONIZED_BIT to tell GL that it
  // we don't care about previous data in the buffer and it shouldn't try to
  // synchronize existing draw calls with it, both things which should be
  // implied anyways since this is a brand new buffer, but we specify just in
  // case.
  mapped_data_ = static_cast<GLubyte*>(glMapBufferRange(
      GL_PIXEL_UNPACK_BUFFER, 0, data_size_, GL_MAP_WRITE_BIT |
                                                 GL_MAP_INVALIDATE_BUFFER_BIT |
                                                 GL_MAP_UNSYNCHRONIZED_BIT));
  CHECK(mapped_data_);
  DCHECK_EQ(GL_NO_ERROR, glGetError());
  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
}

TextureDataPBO::~TextureDataPBO() {
  // mapped_data_ will be non-null if ConvertToTexture() was never called.
  if (mapped_data_) {
    // In the case that we still have valid mapped_data_, we must release it,
    // and we do so on the resource context.
    resource_context_->RunSynchronouslyWithinResourceContext(
        base::Bind(&TextureDataPBO::UnmapAndDeletePBO, base::Unretained(this)));
  }
}

void TextureDataPBO::UnmapAndDeletePBO() {
  resource_context_->AssertWithinResourceContext();

  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer_));
  GL_CALL(glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER));
  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
  GL_CALL(glDeleteBuffers(1, &pixel_buffer_));
}

GLuint TextureDataPBO::ConvertToTexture(GraphicsContextEGL* graphics_context,
                                        bool bgra_supported) {
  DCHECK(mapped_data_) << "ConvertToTexture() should only ever be called once.";

  // Since we wish to create a texture within the specified graphics context,
  // we do the texture creation work using that context.
  GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(graphics_context);

  // Our texture data remains mapped until this method is called, so we begin
  // by unmapping it.
  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer_));
  GL_CALL(glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER));

  // Setup our texture.
  GLuint texture_handle;
  GL_CALL(glGenTextures(1, &texture_handle));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, texture_handle));
  SetupInitialTextureParameters();

  // Determine the texture's GL format.
  if (format_ == GL_BGRA_EXT) {
    DCHECK(bgra_supported);
  }

  // Create the texture.  Since a GL_PIXEL_UNPACK_BUFFER is currently bound,
  // the "data" field passed to glTexImage2D will refer to an offset into that
  // buffer.
  GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH,
                        GetPitchInBytes() / BytesPerPixelForGLFormat(format_)));
  glTexImage2D(GL_TEXTURE_2D, 0, format_, size_.width(), size_.height(), 0,
               format_, GL_UNSIGNED_BYTE, 0);
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Error calling glTexImage2D(size = (" << size_.width() << ", "
               << size_.height() << "))";
    GL_CALL(glDeleteTextures(1, &texture_handle));
    // 0 is considered by GL to be an invalid handle.
    texture_handle = 0;
  }

  GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));

  // The buffer is now referenced by the texture, and we no longer need to
  // explicitly reference it.
  GL_CALL(glDeleteBuffers(1, &pixel_buffer_));
  mapped_data_ = NULL;

  return texture_handle;
}

bool TextureDataPBO::CreationError() { return error_; }

RawTextureMemoryPBO::RawTextureMemoryPBO(ResourceContext* resource_context,
                                         size_t size_in_bytes, size_t alignment)
    : size_in_bytes_(size_in_bytes),
      alignment_(alignment),
      resource_context_(resource_context) {
  resource_context_->RunSynchronouslyWithinResourceContext(
      base::Bind(&RawTextureMemoryPBO::InitAndMapPBO, base::Unretained(this)));
}

void RawTextureMemoryPBO::InitAndMapPBO() {
  resource_context_->AssertWithinResourceContext();

  int size_to_allocate = size_in_bytes_ + alignment_;

  // Allocate a GL pixel unpack buffer with the specified size, leaving enough
  // room such that we can adjust for alignment.
  GL_CALL(glGenBuffers(1, &pixel_buffer_));
  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer_));
  GL_CALL(glBufferData(GL_PIXEL_UNPACK_BUFFER, size_to_allocate, 0,
                       GL_STATIC_DRAW));

  // Map the GL pixel buffer data to CPU addressable memory.  We pass the flags
  // MAP_INVALIDATE_BUFFER_BIT | MAP_UNSYNCHRONIZED_BIT to tell GL that it
  // we don't care about previous data in the buffer and it shouldn't try to
  // synchronize existing draw calls with it, both things which should be
  // implied anyways since this is a brand new buffer, but we specify just in
  // case.
  mapped_data_ = static_cast<GLubyte*>(
      glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, size_to_allocate,
                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT |
                           GL_MAP_UNSYNCHRONIZED_BIT));
  DCHECK_EQ(GL_NO_ERROR, glGetError());
  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));

  // Finally, determine the offset within |mapped_data_| necessary to acheive
  // the desired alignment.
  intptr_t alignment_remainder =
      reinterpret_cast<size_t>(mapped_data_) % alignment_;
  alignment_offset_ =
      alignment_remainder == 0 ? 0 : alignment_ - alignment_remainder;
}

RawTextureMemoryPBO::~RawTextureMemoryPBO() {
  resource_context_->RunSynchronouslyWithinResourceContext(
      base::Bind(&RawTextureMemoryPBO::DestroyPBO, base::Unretained(this)));
}

void RawTextureMemoryPBO::DestroyPBO() {
  resource_context_->AssertWithinResourceContext();

  if (mapped_data_) {
    // mapped_data_ will be non-null if we never call MakeConst() on it, in
    // which case we should unmap it before continuing.
    GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer_));
    GL_CALL(glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER));
    GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));

    mapped_data_ = NULL;
  }

  // Clear our reference to the specified pixel buffer.
  GL_CALL(glDeleteBuffers(1, &pixel_buffer_));
}

void RawTextureMemoryPBO::MakeConst() {
  DCHECK(mapped_data_) << "MakeConst() should only ever be called once.";

  resource_context_->RunSynchronouslyWithinResourceContext(
      base::Bind(&RawTextureMemoryPBO::UnmapPBO, base::Unretained(this)));
}

void RawTextureMemoryPBO::UnmapPBO() {
  resource_context_->AssertWithinResourceContext();

  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer_));
  GL_CALL(glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER));
  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));

  mapped_data_ = NULL;
}

GLuint RawTextureMemoryPBO::CreateTexture(GraphicsContextEGL* graphics_context,
                                          intptr_t offset,
                                          const math::Size& size, GLenum format,
                                          int pitch_in_bytes,
                                          bool bgra_supported) const {
  DCHECK(!mapped_data_)
      << "MakeConst() should be called before calling CreateTexture().";

  GLuint texture_handle;

  // Since we wish to create a texture within the specified graphics context,
  // we do the texture creation work using that context.
  GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(graphics_context);

  // Our texture data remains mapped until this method is called, so we begin
  // by unmapping it.
  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer_));

  // Setup our texture.
  GL_CALL(glGenTextures(1, &texture_handle));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, texture_handle));
  SetupInitialTextureParameters();

  // Determine the texture's GL format.
  if (format == GL_BGRA_EXT) {
    DCHECK(bgra_supported);
  }

  // Create the texture.  Since a GL_PIXEL_UNPACK_BUFFER is currently bound,
  // the "data" field passed to glTexImage2D will refer to an offset into that
  // buffer.
  GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH,
                        pitch_in_bytes / BytesPerPixelForGLFormat(format)));
  GL_CALL(
      glTexImage2D(GL_TEXTURE_2D, 0, format, size.width(), size.height(), 0,
                   format, GL_UNSIGNED_BYTE,
                   reinterpret_cast<const void*>(alignment_offset_ + offset)));
  GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  GL_CALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));

  return texture_handle;
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // defined(GLES3_SUPPORTED)
