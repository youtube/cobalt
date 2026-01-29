// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/experimental/media_buffer_pool.h"

#include "starboard/common/log.h"
#include "starboard/system.h"

namespace starboard {
namespace common {
namespace experimental {

// static
MediaBufferPool* MediaBufferPool::Acquire() {
  const StarboardExtensionMediaBufferPoolApi* api =
      static_cast<const StarboardExtensionMediaBufferPoolApi*>(
          SbSystemGetExtension(kStarboardExtensionMediaBufferPoolApiName));
  if (!api) {
    return nullptr;
  }
  static MediaBufferPool instance(api);
  return &instance;
}

MediaBufferPool::MediaBufferPool(
    const StarboardExtensionMediaBufferPoolApi* api)
    : api_(api) {}

void MediaBufferPool::ShrinkToZero() {
  api_->ShrinkToZero();
}

bool MediaBufferPool::ExpandTo(size_t size) {
  return api_->ExpandTo(size);
}

void MediaBufferPool::Write(intptr_t position, const void* data, size_t size) {
  SB_DCHECK(IsPointerAnnotated(position));

  // |position| is unannotated here, instead of in the extension, as this class
  // is supposed to abstract out all such details from the extension.
  position = UnannotatePointer(position);

  api_->Write(position, data, size);
}

}  // namespace experimental
}  // namespace common
}  // namespace starboard
