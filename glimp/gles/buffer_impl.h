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

namespace glimp {
namespace gles {

// Implements functionality required to support GL memory buffers.  These
// buffers are typically used to store vertex data and vertex index data,
// however it can also be used to store texture data that can later be used
// to create a texture.
class BufferImpl {
 public:
  // Corresponds to the |target| GLenum passed into glBindBuffers().
  enum TargetType {
    kArrayBuffer,
    kElementArrayBuffer,
  };

  // Corresponds to the |usage| GLenum passed into glBufferData().
  enum Usage {
    kStaticDraw,
    kDynamicDraw,
  };

  virtual ~BufferImpl() {}

  // Upload the specified data into this buffer.  |target| and |usage| may
  // both be used as hints for how the data will be used.
  // |target| is specified by the last call to glBindBuffers() and |usage|
  // is specified by the call to glBufferData().  This method is called when
  // glBufferData() is called.
  //   https://www.khronos.org/opengles/sdk/1.1/docs/man/glBufferData.xml
  virtual void SetData(TargetType target,
                       Usage usage,
                       const void* data,
                       size_t size) = 0;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_BUFFER_IMPL_H_
