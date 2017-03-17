// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/egl/rect_allocator.h"

#include <algorithm>

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
bool FirstRectIsSmaller(const math::Rect& a, const math::Rect& b) {
  return a.width() < b.width() || a.height() < b.height();
}
}  // namespace

RectAllocator::RectAllocator() {
  Reset(math::Size(0, 0));
}

RectAllocator::RectAllocator(const math::Size& total_size) {
  Reset(total_size);
}

void RectAllocator::Reset(const math::Size& total_size) {
  total_size_ = total_size;

  unused_.clear();
  unused_.push_back(
      math::Rect(0, 0, total_size_.width(), total_size_.height()));
}

math::Rect RectAllocator::Allocate(const math::Size& alloc_size) {
  math::Rect allocation(0, 0, alloc_size.width(), alloc_size.height());
  MemoryList::iterator pos = std::lower_bound(unused_.begin(), unused_.end(),
                                              allocation, FirstRectIsSmaller);
  if (pos != unused_.end()) {
    // Found an unused block big enough for the allocation.
    math::Rect memory = *pos;
    unused_.erase(pos);

    // Use top-left of memory block for the allocation, and return the bottom
    // and right portions as unused blocks.
    // +-------+-------+
    // | Alloc | Right |
    // +-------+-------+
    // |    Bottom     |
    // +---------------+
    allocation.set_x(memory.x());
    allocation.set_y(memory.y());

    if (memory.width() != allocation.width()) {
      if (memory.height() != allocation.height()) {
        // Return bottom and right of memory block as unused.
        math::Rect right(memory.x() + allocation.width(), memory.y(),
                         memory.width() - allocation.width(),
                         allocation.height());
        AddUnused(right);
        memory.set_y(memory.y() + allocation.height());
        memory.set_height(memory.height() - allocation.height());
        AddUnused(memory);
      } else {
        // Return right of memory block as unused.
        memory.set_x(memory.x() + allocation.width());
        memory.set_width(memory.width() - allocation.width());
        AddUnused(memory);
      }
    } else if (memory.height() != allocation.height()) {
      // Return bottom of memory block as unused.
      memory.set_y(memory.y() + allocation.height());
      memory.set_height(memory.height() - allocation.height());
      AddUnused(memory);
    }

    return allocation;
  } else {
    // Not enough unused space for the allocation.
    return math::Rect(0, 0, 0, 0);
  }
}

void RectAllocator::AddUnused(const math::Rect& memory) {
  DCHECK_LE(memory.right(), total_size_.width());
  DCHECK_LE(memory.bottom(), total_size_.height());
  MemoryList::iterator pos = std::lower_bound(unused_.begin(), unused_.end(),
                                              memory, FirstRectIsSmaller);
  unused_.insert(pos, memory);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
