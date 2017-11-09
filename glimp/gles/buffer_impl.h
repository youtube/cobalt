/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_GLES_BUFFER_IMPL_H_
#define GLIMP_GLES_BUFFER_IMPL_H_

#include <KHR/khrplatform.h>

namespace glimp {
namespace gles {

// Implements functionality required to support GL memory buffers.  These
// buffers are typically used to store vertex data and vertex index data,
// however it can also be used to store texture data that can later be used
// to create a texture.
class BufferImpl {
 public:
  // Corresponds to the |usage| GLenum passed into glBufferData().
  enum Usage {
    kStreamDraw,
    kStaticDraw,
    kDynamicDraw,
  };

  virtual ~BufferImpl() {}

  // Sets the size and usage of allocated memory for the buffer.
  // Since no data is available at this point, implementations are free to
  // delay the actual allocation of memory until data is provided.  Called from
  // glBufferData().  Returns true on success and false on failure.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glBufferData.xml
  virtual bool Allocate(Usage usage, size_t size) = 0;

  // Upload the specified data into this buffer.  Allocate() must have
  // previously been called for a call to SetData() to be valid.
  // This method is called by glBufferData() (when data is not NULL) and
  // glBufferSubData().  Returns true on success and false on an allocation
  // failure.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glBufferData.xml
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glBufferSubData.xml
  virtual bool SetData(intptr_t offset, size_t size, const void* data) = 0;

  // Maps the buffer's data to a CPU-addressible memory location and then
  // returns a pointer to that location.  This method is called when
  // glMapBufferRange() is called.  glimp only supports calls to
  // glMapBufferRange() with the flag GL_MAP_INVALIDATE_BUFFER_BIT set,
  // therefore this method is free to throw away any existing memory when
  // this method is called.
  //   https://www.khronos.org/opengles/sdk/docs/man3/html/glMapBufferRange.xhtml
  virtual void* Map() = 0;

  // Unmaps the data previously mapped to a CPU-addressible memory location
  // through a call to Map().  This method will be called when
  // glUnmapBuffer() is called.  This command should return false if the
  // underlying buffer resource has become invalid while the memory was mapped.
  // Otherwise, it should return true.
  //   https://www.khronos.org/opengles/sdk/docs/man3/html/glMapBufferRange.xhtml
  virtual bool Unmap() = 0;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_BUFFER_IMPL_H_
