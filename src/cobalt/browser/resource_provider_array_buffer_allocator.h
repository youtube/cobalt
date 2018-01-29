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

#ifndef COBALT_BROWSER_RESOURCE_PROVIDER_ARRAY_BUFFER_ALLOCATOR_H_
#define COBALT_BROWSER_RESOURCE_PROVIDER_ARRAY_BUFFER_ALLOCATOR_H_

#include "cobalt/dom/array_buffer.h"
#include "cobalt/render_tree/resource_provider.h"
#include "nb/memory_pool.h"
#include "starboard/common/locked_ptr.h"

namespace cobalt {
namespace browser {

class ResourceProviderArrayBufferAllocator
    : public dom::ArrayBuffer::Allocator {
 public:
  static const size_t kPoolSize = 32 * 1024 * 1024;
  static const size_t kAlignment = 128;

  explicit ResourceProviderArrayBufferAllocator(
      render_tree::ResourceProvider* resource_provider);

 private:
  void* Allocate(size_t size) override;
  void Free(void* p) override;

  scoped_ptr<render_tree::RawImageMemory> gpu_memory_buffer_space_;
  starboard::LockedPtr<nb::FirstFitMemoryPool> gpu_memory_pool_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_RESOURCE_PROVIDER_ARRAY_BUFFER_ALLOCATOR_H_
