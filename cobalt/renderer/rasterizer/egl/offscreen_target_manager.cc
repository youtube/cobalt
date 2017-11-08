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

#include "cobalt/renderer/rasterizer/egl/offscreen_target_manager.h"

#include <algorithm>
#include <unordered_map>

#include "cobalt/renderer/rasterizer/egl/rect_allocator.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {

struct AllocationMapValue {
  AllocationMapValue(const OffscreenTargetManager::ErrorData& in_error_data,
                     const math::RectF& in_target_region)
      : error_data(in_error_data), target_region(in_target_region) {}
  OffscreenTargetManager::ErrorData error_data;
  math::RectF target_region;
};

typedef std::unordered_multimap<int64_t, AllocationMapValue> AllocationMap;

int32_t NextPowerOf2(int32_t num) {
  // Return the smallest power of 2 that is greater than or equal to num.
  // This flips on all bits <= num, then num+1 will be the next power of 2.
  --num;
  num |= num >> 1;
  num |= num >> 2;
  num |= num >> 4;
  num |= num >> 8;
  num |= num >> 16;
  return num + 1;
}

bool IsPowerOf2(int32_t num) {
  return (num & (num - 1)) == 0;
}

size_t GetMemorySize(const math::Size& target_size) {
  // RGBA uses 4 bytes per pixel. Assume no rounding to the nearest power of 2.
  return target_size.width() * target_size.height() * 4;
}

}  // namespace

struct OffscreenTargetManager::OffscreenAtlas {
  explicit OffscreenAtlas(const math::Size& size)
      : allocator(size),
        allocations_used(0),
        needs_flush(false) {}

  RectAllocator allocator;
  AllocationMap allocation_map;
  size_t allocations_used;
  scoped_refptr<backend::FramebufferRenderTargetEGL> framebuffer;
  SkAutoTUnref<SkSurface> skia_surface;
  bool needs_flush;
};

OffscreenTargetManager::OffscreenTargetManager(
    backend::GraphicsContextEGL* graphics_context,
    const CreateFallbackSurfaceFunction& create_fallback_surface,
    size_t memory_limit)
    : graphics_context_(graphics_context),
      create_fallback_surface_(create_fallback_surface),
      offscreen_target_size_mask_(0, 0),
      memory_limit_(memory_limit) {
}

OffscreenTargetManager::~OffscreenTargetManager() {
}

void OffscreenTargetManager::Update(const math::Size& frame_size) {
  if (offscreen_atlases_.empty()) {
    DCHECK(offscreen_atlases_1d_.empty());
    InitializeTargets(frame_size);
  }

  SelectAtlasCache(&offscreen_atlases_, &offscreen_cache_);
  SelectAtlasCache(&offscreen_atlases_1d_, &offscreen_cache_1d_);

  // Delete uncached targets that were not used in the previous render frame.
  for (size_t index = 0; index < uncached_targets_.size();) {
    if (uncached_targets_[index]->allocations_used == 0) {
      uncached_targets_.erase(uncached_targets_.begin() + index);
    } else {
      uncached_targets_[index]->allocations_used = 0;
      ++index;
    }
  }
}

void OffscreenTargetManager::SelectAtlasCache(
    ScopedVector<OffscreenAtlas>* atlases_ptr,
    scoped_ptr<OffscreenAtlas>* cache_ptr) {
  ScopedVector<OffscreenAtlas>& atlases = *atlases_ptr;
  scoped_ptr<OffscreenAtlas>& cache = *cache_ptr;

  // If any of the current atlases have more allocations used than the
  // current cache, then use that as the new cache.
  size_t most_used_atlas_index = 0;
  for (size_t index = 1; index < atlases.size(); ++index) {
    if (atlases[most_used_atlas_index]->allocations_used <
        atlases[index]->allocations_used) {
      most_used_atlas_index = index;
    }
  }

  OffscreenAtlas* most_used_atlas = atlases[most_used_atlas_index];
  if (cache->allocations_used < most_used_atlas->allocations_used) {
    OffscreenAtlas* new_atlas = cache.release();
    cache.reset(atlases[most_used_atlas_index]);
    atlases.weak_erase(atlases.begin() + most_used_atlas_index);
    atlases.push_back(new_atlas);
  }
  cache->allocations_used = 0;

  // Reset all current atlases for use this frame.
  for (size_t index = 0; index < atlases.size(); ++index) {
    OffscreenAtlas* atlas = atlases[index];
    atlas->allocator.Reset();
    atlas->allocation_map.clear();
    atlas->allocations_used = 0;
  }
}

void OffscreenTargetManager::Flush() {
  if (offscreen_cache_->needs_flush) {
    offscreen_cache_->needs_flush = false;
    offscreen_cache_->skia_surface->getCanvas()->flush();
  }
  for (size_t index = 0; index < offscreen_atlases_.size(); ++index) {
    if (offscreen_atlases_[index]->needs_flush) {
      offscreen_atlases_[index]->needs_flush = false;
      offscreen_atlases_[index]->skia_surface->getCanvas()->flush();
    }
  }
}

bool OffscreenTargetManager::GetCachedTarget(const render_tree::Node* node,
    const CacheErrorFunction& error_function, TargetInfo* out_target_info) {
  // Find the cache of the given node (if any) with the lowest error.
  AllocationMap::iterator best_iter = offscreen_cache_->allocation_map.end();
  float best_error = 2.0f;

  auto range = offscreen_cache_->allocation_map.equal_range(node->GetId());
  for (auto iter = range.first; iter != range.second; ++iter) {
    float error = error_function.Run(iter->second.error_data);
    if (best_error > error) {
      best_error = error;
      best_iter = iter;
    }
  }

  // A cache entry matches the caller's criteria only if error < 1.
  if (best_error < 1.0f) {
    offscreen_cache_->allocations_used += 1;
    out_target_info->framebuffer = offscreen_cache_->framebuffer.get();
    out_target_info->skia_canvas = offscreen_cache_->skia_surface->getCanvas();
    out_target_info->region = best_iter->second.target_region;
    return true;
  }

  return false;
}

bool OffscreenTargetManager::GetCachedTarget(const render_tree::Node* node,
    const CacheErrorFunction1D& error_function, TargetInfo* out_target_info) {
  // Find the cache of the given node (if any) with the lowest error.
  AllocationMap::iterator best_iter = offscreen_cache_1d_->allocation_map.end();
  float best_error = 2.0f;

  auto range = offscreen_cache_1d_->allocation_map.equal_range(node->GetId());
  for (auto iter = range.first; iter != range.second; ++iter) {
    float error = error_function.Run(iter->second.error_data.width());
    if (best_error > error) {
      best_error = error;
      best_iter = iter;
    }
  }

  // A cache entry matches the caller's criteria only if error < 1.
  if (best_error < 1.0f) {
    offscreen_cache_1d_->allocations_used += 1;
    out_target_info->framebuffer = offscreen_cache_1d_->framebuffer.get();
    out_target_info->skia_canvas = nullptr;
    out_target_info->region = best_iter->second.target_region;
    return true;
  }

  return false;
}

void OffscreenTargetManager::AllocateCachedTarget(
    const render_tree::Node* node, const math::SizeF& size,
    const ErrorData& error_data, TargetInfo* out_target_info) {
  // Pad the offscreen target size to prevent interpolation with unwanted
  // texels when rendering the results.
  const int kInterpolatePad = 1;

  // Get an offscreen target for rendering. Align up the requested target size
  // to improve usage of the atlas (since more requests will have the same
  // aligned width or height).
  math::Size target_size(
      (static_cast<int>(std::ceil(size.width())) + 2 * kInterpolatePad +
          offscreen_target_size_mask_.width()) &
          ~offscreen_target_size_mask_.width(),
      (static_cast<int>(std::ceil(size.height())) + 2 * kInterpolatePad +
          offscreen_target_size_mask_.height()) &
          ~offscreen_target_size_mask_.height());
  math::RectF target_rect(0.0f, 0.0f, 0.0f, 0.0f);
  OffscreenAtlas* atlas = NULL;

  // See if there's room in the offscreen cache for additional targets.
  atlas = offscreen_cache_.get();
  target_rect = atlas->allocator.Allocate(target_size);

  if (target_rect.IsEmpty()) {
    // See if there's room in the other atlases.
    for (size_t index = offscreen_atlases_.size(); index > 0;) {
      atlas = offscreen_atlases_[--index];
      target_rect = atlas->allocator.Allocate(target_size);
      if (!target_rect.IsEmpty()) {
        break;
      }
    }
  }

  if (target_rect.IsEmpty()) {
    // There wasn't enough room for the requested offscreen target.
    out_target_info->framebuffer = nullptr;
    out_target_info->skia_canvas = nullptr;
    out_target_info->region.SetRect(0, 0, 0, 0);
  } else {
    // Inset to prevent interpolation with unwanted pixels at the edge.
    target_rect.Inset(kInterpolatePad, kInterpolatePad);
    DCHECK_LE(size.width(), target_rect.width());
    DCHECK_LE(size.height(), target_rect.height());
    target_rect.set_size(size);

    // Clear the atlas if this will be the first draw into it.
    if (atlas->allocation_map.empty()) {
      atlas->skia_surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    }

    atlas->allocation_map.insert(AllocationMap::value_type(
        node->GetId(), AllocationMapValue(error_data, target_rect)));
    atlas->allocations_used += 1;
    atlas->needs_flush = true;

    out_target_info->framebuffer = atlas->framebuffer.get();
    out_target_info->skia_canvas = atlas->skia_surface->getCanvas();
    out_target_info->region = target_rect;
  }
}

void OffscreenTargetManager::AllocateCachedTarget(
    const render_tree::Node* node, float size,
    const ErrorData1D& error_data, TargetInfo* out_target_info) {
  // 1D targets do not use any padding to avoid interpolation with neighboring
  // allocations in the atlas.
  math::Size target_size(static_cast<int>(std::ceil(size)), 1);
  math::RectF target_rect(0.0f, 0.0f, 0.0f, 0.0f);
  OffscreenAtlas* atlas = NULL;

  // See if there's room in the offscreen cache for additional targets.
  atlas = offscreen_cache_1d_.get();
  target_rect = atlas->allocator.Allocate(target_size);

  if (target_rect.IsEmpty()) {
    // See if there's room in the other atlases.
    for (size_t index = offscreen_atlases_1d_.size(); index > 0;) {
      atlas = offscreen_atlases_1d_[--index];
      target_rect = atlas->allocator.Allocate(target_size);
      if (!target_rect.IsEmpty()) {
        break;
      }
    }
  }

  if (target_rect.IsEmpty()) {
    // There wasn't enough room for the requested offscreen target.
    out_target_info->framebuffer = nullptr;
    out_target_info->skia_canvas = nullptr;
    out_target_info->region.SetRect(0, 0, 0, 0);
  } else {
    DCHECK_LE(size, target_rect.width());
    target_rect.set_width(size);

    atlas->allocation_map.insert(AllocationMap::value_type(
        node->GetId(), AllocationMapValue(
            ErrorData(error_data, 1), target_rect)));
    atlas->allocations_used += 1;
    atlas->needs_flush = false;

    out_target_info->framebuffer = atlas->framebuffer.get();
    out_target_info->skia_canvas = nullptr;
    out_target_info->region = target_rect;
  }
}

void OffscreenTargetManager::AllocateUncachedTarget(const math::SizeF& size,
    TargetInfo* out_target_info) {
  // Align up the requested target size to increase the chances that it can
  // be reused in subsequent frames.
  math::Size target_size(
      (static_cast<int>(std::ceil(size.width())) +
          offscreen_target_size_mask_.width()) &
          ~offscreen_target_size_mask_.width(),
      (static_cast<int>(std::ceil(size.height())) +
          offscreen_target_size_mask_.height()) &
          ~offscreen_target_size_mask_.height());

  // Find a render target that meets the size requirement. However, do not
  // select anything that is too big.
  const int kMaxTargetSize = target_size.GetArea() * 2;
  OffscreenAtlas* atlas = nullptr;

  for (size_t index = 0; index < uncached_targets_.size(); ++index) {
    OffscreenAtlas* current = uncached_targets_[index];
    if (current->allocations_used == 0) {
      const math::Size& current_size = current->framebuffer->GetSize();
      if (current_size.width() >= target_size.width() &&
          current_size.height() >= target_size.height() &&
          current_size.GetArea() <= kMaxTargetSize) {
        // Pick the smallest render target that meets the requirements.
        if (atlas && atlas->framebuffer->GetSize().GetArea() <
            current_size.GetArea()) {
          continue;
        }
        atlas = current;
      }
    }
  }

  if (!atlas) {
    atlas = CreateOffscreenAtlas(target_size, true).release();
    if (!atlas) {
      // If there was an error allocating the offscreen atlas, indicate by
      // marking framebuffer and skia canvas as null and returning early.
      out_target_info->framebuffer = nullptr;
      out_target_info->skia_canvas = nullptr;
      return;
    }
    uncached_targets_.push_back(atlas);
  }

  atlas->allocations_used = 1;
  out_target_info->framebuffer = atlas->framebuffer.get();
  out_target_info->skia_canvas = atlas->skia_surface->getCanvas();
  out_target_info->region.SetRect(0.0f, 0.0f, size.width(), size.height());
}

void OffscreenTargetManager::InitializeTargets(const math::Size& frame_size) {
  DLOG(INFO) << "offscreen render target memory limit: " << memory_limit_;

  if (frame_size.width() >= 64 && frame_size.height() >= 64) {
    offscreen_target_size_mask_.SetSize(
        NextPowerOf2(frame_size.width() / 64) - 1,
        NextPowerOf2(frame_size.height() / 64) - 1);
  } else {
    offscreen_target_size_mask_.SetSize(0, 0);
  }
  DCHECK(IsPowerOf2(offscreen_target_size_mask_.width() + 1));
  DCHECK(IsPowerOf2(offscreen_target_size_mask_.height() + 1));

  // Allow offscreen targets to be as large as the frame.
  math::Size max_size(std::max(frame_size.width(), 1),
                      std::max(frame_size.height(), 1));

  // Offscreen render targets are optional but highly recommended. These
  // allow caching of render results for improved performance. At least two
  // must exist -- one for the cache and the other a working scratch.
  size_t half_memory_limit = memory_limit_ / 2;
  math::Size atlas_size(1, 1);
  for (;;) {
    // See if the next atlas size will fit in the memory budget.
    // Try to keep the atlas square-ish.
    math::Size next_size(atlas_size);
    if (atlas_size.width() < max_size.width() &&
        (atlas_size.width() <= atlas_size.height() ||
        atlas_size.height() >= max_size.height())) {
      next_size.set_width(
          std::min(atlas_size.width() * 2, max_size.width()));
    } else if (atlas_size.height() < max_size.height()) {
      next_size.set_height(
          std::min(atlas_size.height() * 2, max_size.height()));
    } else {
      break;
    }
    if (GetMemorySize(next_size) > half_memory_limit) {
      break;
    }
    atlas_size = next_size;
  }

  // It is better to have fewer, large atlases than many small atlases to
  // minimize the cost of switching render targets. Consider changing the
  // max_size logic if there's plenty of memory to spare.
  const int kMaxAtlases = 4;
  int num_atlases = memory_limit_ / GetMemorySize(atlas_size);
  if (num_atlases < 2) {
    // Must have at least two atlases -- even if they are of a token size.
    // This simplifies code elsewhere.
    DCHECK(atlas_size.width() == 1 && atlas_size.height() == 1);
    num_atlases = 2;
  } else if (num_atlases > kMaxAtlases) {
    DCHECK(atlas_size == max_size);
    num_atlases = kMaxAtlases;
    DLOG(WARNING) << "More memory was allotted for offscreen render targets"
                  << " than will be used.";
  }
  offscreen_cache_ = CreateOffscreenAtlas(atlas_size, true);
  CHECK(offscreen_cache_);
  for (int i = 1; i < num_atlases; ++i) {
    offscreen_atlases_.push_back(
        CreateOffscreenAtlas(atlas_size, true).release());
    CHECK(offscreen_atlases_.back());
  }

  DLOG(INFO) << "Created " << num_atlases << " offscreen atlases of size "
             << atlas_size.width() << " x " << atlas_size.height();

  // Create 1D texture atlases. These are just regular 2D textures that will
  // be used as 1D row textures. These atlases are not intended to be used by
  // skia.
  const int kAtlasHeight1D =
      std::min(std::min(16, frame_size.width()), frame_size.height());
  math::Size atlas_size_1d(
      std::max(frame_size.width(), frame_size.height()), kAtlasHeight1D);
  offscreen_atlases_1d_.push_back(
      CreateOffscreenAtlas(atlas_size_1d, false).release());
  CHECK(offscreen_atlases_1d_.back());
  offscreen_cache_1d_ = CreateOffscreenAtlas(atlas_size_1d, false);
  CHECK(offscreen_cache_1d_);
  DLOG(INFO) << "Created " << offscreen_atlases_1d_.size() + 1
             << " offscreen atlases of size " << atlas_size_1d.width() << " x "
             << atlas_size_1d.height();
}

scoped_ptr<OffscreenTargetManager::OffscreenAtlas>
OffscreenTargetManager::CreateOffscreenAtlas(const math::Size& size,
                                             bool create_canvas) {
  scoped_ptr<OffscreenAtlas> atlas(new OffscreenAtlas(size));

  // Create a new framebuffer.
  atlas->framebuffer = new backend::FramebufferRenderTargetEGL(
      graphics_context_, size);
  if (atlas->framebuffer->CreationError()) {
    return scoped_ptr<OffscreenAtlas>();
  }

  if (create_canvas) {
    // Wrap the framebuffer as a skia surface.
    atlas->skia_surface.reset(create_fallback_surface_.Run(atlas->framebuffer));
  }

  return atlas.Pass();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
