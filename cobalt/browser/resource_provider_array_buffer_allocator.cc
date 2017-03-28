// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/resource_provider_array_buffer_allocator.h"

#include "starboard/common/scoped_ptr.h"

namespace cobalt {
namespace browser {

ResourceProviderArrayBufferAllocator::ResourceProviderArrayBufferAllocator(
    render_tree::ResourceProvider* resource_provider) {
  gpu_memory_buffer_space_ =
      resource_provider->AllocateRawImageMemory(kPoolSize, kAlignment);
  DCHECK(gpu_memory_buffer_space_);
  DCHECK(gpu_memory_buffer_space_->GetMemory());

  gpu_memory_pool_.set(starboard::make_scoped_ptr(
      new nb::MemoryPool(gpu_memory_buffer_space_->GetMemory(),
                         gpu_memory_buffer_space_->GetSizeInBytes())));
}

void* ResourceProviderArrayBufferAllocator::Allocate(size_t size) {
  return gpu_memory_pool_->Allocate(size, kAlignment);
}

void ResourceProviderArrayBufferAllocator::Free(void* p) {
  gpu_memory_pool_->Free(p);
}

}  // namespace browser
}  // namespace cobalt
