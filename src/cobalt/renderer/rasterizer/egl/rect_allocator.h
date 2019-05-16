// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_RECT_ALLOCATOR_H_
#define COBALT_RENDERER_RASTERIZER_EGL_RECT_ALLOCATOR_H_

#include <vector>

#include "cobalt/math/rect.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Manage 2D allocations from a virtual 2D space. This maintains a sorted list
// of unused memory from which allocations are made using a best-fit search.
// If an allocation does not exactly fit a memory block, then the unused
// portions are added to the unused memory list. Upon deallocation, unused
// memory blocks are not merged, so fragmentation will occur quickly.
class RectAllocator {
 public:
  RectAllocator();
  explicit RectAllocator(const math::Size& total_size);

  const math::Size& GetTotalSize() const { return total_size_; }

  // Remove all allocations, and optionally reset the total managed memory.
  void Reset() { Reset(total_size_); }
  void Reset(const math::Size& total_size);

  math::Rect Allocate(const math::Size& alloc_size);
  void Deallocate(const math::Rect& allocation) { AddUnused(allocation); }

 private:
  void AddUnused(const math::Rect& memory);

  math::Size total_size_;  // Total memory space managed by the allocator.

  typedef std::vector<math::Rect> MemoryList;
  MemoryList unused_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_RECT_ALLOCATOR_H_
