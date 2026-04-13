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

#include "starboard/android/shared/media_buffer_pool_extension.h"

#include <stddef.h>
#include <stdint.h>

#include "starboard/android/shared/memfd_media_buffer_pool.h"
#include "starboard/common/log.h"
#include "starboard/extension/experimental/media_buffer_pool.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

void ShrinkToZero() {
  MemFdMediaBufferPool::Get()->ShrinkToZero();
}

bool ExpandTo(size_t size) {
  return MemFdMediaBufferPool::Get()->ExpandTo(size);
}

void Write(intptr_t position, const void* data, size_t size) {
  MemFdMediaBufferPool::Get()->Write(position, data, size);
}

const StarboardExtensionMediaBufferPoolApi kMediaBufferPoolApi = {
    kStarboardExtensionMediaBufferPoolApiName,
    1,
    &ShrinkToZero,
    &ExpandTo,
    &Write,
};

}  // namespace

const void* GetMediaBufferPoolApi() {
  if (!MemFdMediaBufferPool::Get()) {
    return nullptr;
  }
  return &kMediaBufferPoolApi;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
