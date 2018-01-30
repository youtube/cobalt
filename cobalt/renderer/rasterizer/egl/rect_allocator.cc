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
// Sort by descending area then width and height. This will place smaller
// blocks at the end of the free list. Allocations will use the block with
// the smallest area of sufficient dimensions in order to minimize waste.
  bool FirstRectIsBigger(const math::Rect& a, const math::Rect& b) {
    const int area_a = a.width() * a.height();
    const int area_b = b.width() * b.height();
    return (area_a > area_b) || (area_a == area_b && (a.width() > b.width() ||
                                                      a.height() > b.height()));
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
  if ((alloc_size.width() < 0) || (alloc_size.height() < 0)) {
    // Invalid allocation was requested.
    return math::Rect(0, 0, 0, 0);
  }
  math::Rect allocation(0, 0, alloc_size.width(), alloc_size.height());

  // Find the first block that is too small for the requested allocation.
  MemoryList::iterator pos = std::upper_bound(
      unused_.begin(), unused_.end(), allocation, FirstRectIsBigger);

  // All blocks before "pos" will have sufficient area, but a sequential search
  // must be used to find a block of sufficient width and height.
  while (pos != unused_.begin()) {
    --pos;
    if (pos->width() >= allocation.width() &&
        pos->height() >= allocation.height()) {
      // Found an unused block big enough for the allocation.
      math::Rect memory = *pos;
      unused_.erase(pos);

      // Use top-left of memory block for the allocation, and return the
      // bottom and right portions as unused blocks. The split can be done
      // in one of two ways:
      // +-------+-------+      +-------+-------+
      // | Alloc | Right |      | Alloc |       |
      // +-------+-------+  or  +-------+ Right +
      // |    Bottom     |      |Bottom |       |
      // +---------------+      +-------+-------+
      allocation.set_x(memory.x());
      allocation.set_y(memory.y());

      if (memory.width() != allocation.width()) {
        if (memory.height() != allocation.height()) {
          // Return bottom and right of memory block as unused.
          // Preserve the largest block.
          const int remaining_width = memory.width() - allocation.width();
          const int remaining_height = memory.height() - allocation.height();
          if (memory.width() * remaining_height >=
              remaining_width * memory.height()) {
            // Bottom portion is bigger.
            math::Rect right(memory.x() + allocation.width(), memory.y(),
                             remaining_width, allocation.height());
            memory.set_y(memory.y() + allocation.height());
            memory.set_height(remaining_height);
            AddUnused(memory);
            AddUnused(right);
          } else {
            // Right portion is bigger.
            math::Rect bottom(memory.x(), memory.y() + allocation.height(),
                              allocation.width(), remaining_height);
            memory.set_x(memory.x() + allocation.width());
            memory.set_width(remaining_width);
            AddUnused(memory);
            AddUnused(bottom);
          }
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
    }
  }

  // Not enough unused space for the allocation.
  return math::Rect(0, 0, 0, 0);
}

void RectAllocator::AddUnused(const math::Rect& memory) {
  DCHECK_LE(memory.right(), total_size_.width());
  DCHECK_LE(memory.bottom(), total_size_.height());
  MemoryList::iterator pos = std::lower_bound(
      unused_.begin(), unused_.end(), memory, FirstRectIsBigger);
  unused_.insert(pos, memory);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
