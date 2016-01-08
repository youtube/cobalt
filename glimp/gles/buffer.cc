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

#include "glimp/gles/buffer.h"

namespace glimp {
namespace gles {

namespace {

BufferImpl::TargetType GLTargetEnumToTargetType(GLenum target) {
  switch (target) {
    case GL_ARRAY_BUFFER:
      return BufferImpl::kArrayBuffer;
    case GL_ELEMENT_ARRAY_BUFFER:
      return BufferImpl::kElementArrayBuffer;
  }

  SB_NOTREACHED();
  return BufferImpl::kArrayBuffer;
}

BufferImpl::Usage GLUsageEnumToUsage(GLenum usage) {
  switch (usage) {
    case GL_STATIC_DRAW:
      return BufferImpl::kStaticDraw;
    case GL_DYNAMIC_DRAW:
      return BufferImpl::kDynamicDraw;
  }

  SB_NOTREACHED();
  return BufferImpl::kStaticDraw;
}

}  // namespace

Buffer::Buffer(nb::scoped_ptr<BufferImpl> impl)
    : impl_(impl.Pass()), target_valid_(false) {}

void Buffer::SetTarget(GLenum target) {
  target_ = target;
  target_valid_ = true;
}

void Buffer::SetData(GLsizeiptr size, const GLvoid* data, GLenum usage) {
  SB_DCHECK(target_valid());
  impl_->SetData(GLTargetEnumToTargetType(target_), GLUsageEnumToUsage(usage),
                 data, static_cast<size_t>(size));
}

}  // namespace gles
}  // namespace glimp
