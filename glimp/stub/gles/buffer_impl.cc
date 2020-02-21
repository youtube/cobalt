/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "glimp/stub/gles/buffer_impl.h"

#include "starboard/memory.h"

namespace glimp {
namespace gles {

BufferImplStub::BufferImplStub() {}

bool BufferImplStub::Allocate(Usage usage, khronos_usize_t size) {
  return false;
}

bool BufferImplStub::SetData(khronos_intptr_t offset,
                             khronos_usize_t size,
                             const void* data) {
  SB_DCHECK(data);
  return false;
}

void* BufferImplStub::Map() {
  return NULL;
}

bool BufferImplStub::Unmap() {
  return true;
}

}  // namespace gles
}  // namespace glimp
