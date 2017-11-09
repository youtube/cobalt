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

#ifndef GLIMP_GLES_BUFFER_H_
#define GLIMP_GLES_BUFFER_H_

#include <GLES3/gl3.h>

#include "glimp/gles/buffer_impl.h"
#include "nb/ref_counted.h"
#include "nb/scoped_ptr.h"

namespace glimp {
namespace gles {

class Buffer : public nb::RefCountedThreadSafe<Buffer> {
 public:
  explicit Buffer(nb::scoped_ptr<BufferImpl> impl);

  // Allocates memory within this Buffer object.  Returns false if there
  // was an allocation failure.
  bool Allocate(GLenum usage, size_t size);

  // Implements support for glBufferData() on this buffer object.  Returns
  // false if there was an allocation failure.
  bool SetData(GLintptr offset, GLsizeiptr size, const GLvoid* data);

  // Maps the buffer's memory to a CPU-accessible pointer and returns it, or
  // NULL on failure.
  void* Map();
  bool Unmap();

  // Returns true if the buffer is currently mapped to the CPU address space.
  bool is_mapped() const { return is_mapped_; }

  GLsizeiptr size_in_bytes() const { return size_in_bytes_; }

  BufferImpl* impl() const { return impl_.get(); }

 private:
  friend class nb::RefCountedThreadSafe<Buffer>;
  ~Buffer() {}

  nb::scoped_ptr<BufferImpl> impl_;

  // The size of the allocated memory used by this buffer.
  GLsizeiptr size_in_bytes_;

  // Is the buffer's data currently mapped to CPU address space?
  bool is_mapped_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_BUFFER_H_
