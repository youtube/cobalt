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

BufferImpl::Usage GLUsageEnumToUsage(GLenum usage) {
  switch (usage) {
    case GL_STREAM_DRAW:
      return BufferImpl::kStreamDraw;
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
    : impl_(impl.Pass()), size_in_bytes_(0) {}

bool Buffer::Allocate(GLenum usage, size_t size) {
  size_in_bytes_ = size;
  return impl_->Allocate(GLUsageEnumToUsage(usage), size);
}

bool Buffer::SetData(GLintptr offset, GLsizeiptr size, const GLvoid* data) {
  SB_DCHECK(size_in_bytes_ >= offset + size);
  SB_DCHECK(offset >= 0);
  SB_DCHECK(size >= 0);
  SB_DCHECK(!is_mapped_);

  if (size > 0) {
    return impl_->SetData(offset, static_cast<size_t>(size), data);
  } else {
    return true;
  }
}

void* Buffer::Map() {
  SB_DCHECK(!is_mapped_);
  is_mapped_ = true;

  return impl_->Map();
}

bool Buffer::Unmap() {
  SB_DCHECK(is_mapped_);
  is_mapped_ = false;

  return impl_->Unmap();
}

}  // namespace gles
}  // namespace glimp
