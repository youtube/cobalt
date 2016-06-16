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

#include "cobalt/renderer/rasterizer/common/surface_cache.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/debug/trace_event.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {

namespace {
// The fraction of how much each newly measured apply surface duration affects
// the current exponentially-weighted average apply surface duration.
const float kApplySurfaceDurationChangeRate = 0.05f;

// The initial guess for the duration of apply surface.  This will be updated
// as we acquire data, but until then we start with this guess.
const int64 kInitialApplySurfaceTimeInUSecs = 100;

// Since caching elements is expensive, we limit the number of new entries added
// to the cache each frame.
const size_t kMaxElementsToCachePerFrame = 3;

// If a node has been seen for kConsecutiveFramesForPreference consecutive
// frames, it is preferred for being cached over nodes that have been seen less
// consecutive times.  A setting of 2 is qualitatively important because it is
// impossible for an animated node to reach a value of 2 or more, and so by
// setting this value to 2, we will always prefer non-animated nodes over
// animated nodes.
const int kConsecutiveFramesForPreference = 2;

// Part of the hard criteria for caching a node is that it takes longer than
// the time it takes to render a cached surface.  More specifically, a node
// must take A * kApplySurfaceTimeMultipleCacheCriteria time to render in order
// for it to be considered for the cache, where A is the average time to
// render a cached surface.
const int kApplySurfaceTimeMultipleCacheCriteria = 10;

// Similar to the above, if a node's render time falls below the following
// multiple of the apply surface time (which may be change over time), it will
// be considered for purging from the cache.
const int kApplySurfaceTimeMultiplePurgeCriteria = 4;
}  // namespace

void SurfaceCache::Block::InitBlock() {
  start_time_ = base::TimeTicks::Now();

  // Keep track of the stack of blocks currently being rendered.
  parent_ = cache_->current_block_;
  cache_->current_block_ = this;

  math::RectF node_bounds = node_->GetBounds();

  NodeMap::iterator seen_iter = cache_->seen_.find(node_);
  node_data_ = (seen_iter == cache_->seen_.end() ? NULL : &seen_iter->second);

  if (node_data_) {
    // In order for the cache to function, it needs to know which nodes are
    // seen frequently and which are not.  Setting this flag is the main signal.
    node_data_->visited_this_frame = true;

    // Check that the actual size that we are rendering to now is compatible
    // with the size when this node was originally inserted into the cache.  If
    // the sizes are incompatible, remove the old version from the cache so that
    // it can be replaced by this new version.
    // The new size is compatible with the old size if the new size is smaller
    // than the old size, or the new size is smaller than the local size of
    // the node being rendered.
    math::Size render_size =
        cache_->delegate_->GetRenderSize(node_bounds.size());
    if ((render_size.width() > node_bounds.size().width() &&
         render_size.width() > node_data_->render_size.width()) ||
        (render_size.height() > node_bounds.size().height() &&
         render_size.height() > node_data_->render_size.height())) {
      // Sizes are incompatible, remove the old cached version of this node.
      cache_->RemoveFromSeen(seen_iter);
      node_data_ = NULL;
    }
  }

  if (node_data_ && cache_->IsCached(*node_data_)) {
    state_ = kStateAlreadyCached;
    {
      TRACE_EVENT0("cobalt::renderer", "SurfaceCache ApplySurface");
      // If the node is already cached, use the cached version and mark that we
      // were cached so that the caller knows it doesn't need to do anything
      // more.
      cache_->delegate_->ApplySurface(node_data_->surface);
    }

    // Record how long it took to apply the surface so that we can use this
    // information later to determine whether it's worth it or not to cache
    // another block.  Note that we assume that playing back a cached surface
    // takes a constant amount of time, regardless of surface size.
    base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
    cache_->average_apply_surface_time_in_usecs_.AddValue(
        duration.InMicroseconds());

    // Increase the duration of ancestor blocks so that from their perspective
    // it was like we rendered our content without using the cache.
    IncreaseAncestorBlockDurations(node_data_->duration - duration);
  } else {
    // Determine if we should cache the node or not.  Do not record if we are
    // already recording, since this node is therefore implicitly being cached
    // by its ancestor.
    if (!cache_->recording_ && node_data_ &&
        cache_->IsCacheCandidate(*node_data_)) {
      state_ = kStateRecording;
      cache_->recording_ = true;
      cache_->PurgeUntilSpaceAvailable(node_data_->size_in_bytes);

      // Since we are now caching the surface, make sure the recorded render
      // size is up to date as this will reflect the size of the cached surface
      // allocated for this node.
      node_data_->render_size =
          cache_->delegate_->GetRenderSize(node_bounds.size());
      // Signal to the delegate that we would like to record this block for
      // caching.
      cache_->delegate_->StartRecording(node_bounds);
    } else {
      // The node is not cached, and it should not become cached, so just do
      // nothing besides record how long it takes via |start_time_|, set above.
      state_ = kStateNotRecording;
    }
  }
}

namespace {
float MicrosecondsPerPixel(const base::TimeDelta duration,
                           const math::Size& render_size) {
  return static_cast<float>(duration.InMicroseconds()) / render_size.GetArea();
}
int SizeInBytesForArea(const math::Size& size) { return size.GetArea() * 4; }
}  // namespace

void SurfaceCache::Block::ShutdownBlock() {
  DCHECK(cache_);

  switch (state_) {
    case kStateAlreadyCached: {
      // Nothing to do.
    } break;
    case kStateRecording: {
      // Leave it to the delegate to conclude the recording process.
      DCHECK(node_data_);
      cache_->SetCachedSurface(node_data_, cache_->delegate_->EndRecording());
      // Immediately apply the recorded surface to the outer surface.
      cache_->delegate_->ApplySurface(node_data_->surface);
      cache_->recording_ = false;

      base::TimeDelta duration = base::TimeTicks::Now() - start_time_;

      // Adjust our ancestor block durations so that from their perspective it
      // was as if we did not perform any extra recording logic.
      IncreaseAncestorBlockDurations(node_data_->duration - duration);
    } break;
    case kStateNotRecording: {
      // We're done now, get metrics from the delegate and determine whether or
      // not we should cache the result for next time.
      base::TimeDelta duration = base::TimeTicks::Now() - start_time_;

      // Record that we've seen this block before, if we haven't already done
      // so.
      if (!node_data_) {
        std::pair<NodeMap::iterator, bool> insert_results =
            cache_->seen_.insert(std::make_pair(node_, NodeData(node_)));
        DCHECK(insert_results.second);
        node_data_ = &insert_results.first->second;
      }

      // Update the recorded entry with new timing information.
      DCHECK_LT(0.0f, node_->GetBounds().size().GetArea());
      node_data_->render_size =
          cache_->delegate_->GetRenderSize(node_->GetBounds().size());
      node_data_->duration = duration;
      node_data_->microseconds_per_pixel =
          MicrosecondsPerPixel(duration, node_data_->render_size);
      node_data_->size_in_bytes = SizeInBytesForArea(node_data_->render_size);
    } break;
    case kStateInvalid: {
      NOTREACHED();
    } break;
  }

  // This block is done, pop it off of our block stack.
  DCHECK_EQ(this, cache_->current_block_);
  cache_->current_block_ = parent_;
}

void SurfaceCache::Block::IncreaseAncestorBlockDurations(
    const base::TimeDelta& duration) {
  // For all blocks currently on the stack, move BACK their start time so
  // that their durations will be correspondingly increased by the indicated
  // amount.
  Block* block = parent_;
  while (block != NULL) {
    block->start_time_ -= duration;
    block = block->parent_;
  }
}

SurfaceCache::SurfaceCache(Delegate* delegate, size_t capacity_in_bytes)
    : delegate_(delegate),
      total_used_bytes_(
          "Renderer.SurfaceCache.BytesUsed", 0,
          "Total number of bytes currently used by the surface caching "
          "system."),
      capacity_in_bytes_(capacity_in_bytes),
      recording_(false),
      current_block_(NULL),
      average_apply_surface_time_in_usecs_(kApplySurfaceDurationChangeRate),
      maximum_surface_size_(delegate_->MaximumSurfaceSize()) {
  // It will be updated while the application runs, but for now, initialize the
  // time to apply a surface to a default initial value.
  average_apply_surface_time_in_usecs_.AddValue(
      kInitialApplySurfaceTimeInUSecs);
}

// Sort NodeData objects in descending order by microseconds per second.  Prefer
// nodes that have been seen multiple frames in a row to appear before those
// that haven't.
class SurfaceCache::SortByDurationPerPixel {
 public:
  bool operator()(const NodeData* lhs, const NodeData* rhs) const {
    if (lhs->consecutive_frames_visited_ >= kConsecutiveFramesForPreference !=
        rhs->consecutive_frames_visited_ >= kConsecutiveFramesForPreference) {
      return lhs->consecutive_frames_visited_ >=
             kConsecutiveFramesForPreference;
    }
    return lhs->microseconds_per_pixel > rhs->microseconds_per_pixel;
  }
};

// Sort NodeData objects in descending order by duration to render.  Prefer
// nodes that have been seen multiple frames in a row to appear before those
// that haven't.
class SurfaceCache::SortByDuration {
 public:
  bool operator()(const NodeData* lhs, const NodeData* rhs) const {
    if (lhs->consecutive_frames_visited_ >= kConsecutiveFramesForPreference !=
        rhs->consecutive_frames_visited_ >= kConsecutiveFramesForPreference) {
      return lhs->consecutive_frames_visited_ >=
             kConsecutiveFramesForPreference;
    }
    return lhs->duration > rhs->duration;
  }
};

void SurfaceCache::Frame() {
  TRACE_EVENT0("cobalt::renderer", "SurfaceCache::Frame()");

  // We are about to determine what will potentially appear in the cache for
  // the next frame.  |candidate_cache_size| will keep track of how many bytes
  // will be in cache.
  int candidate_cache_size = 0;

  // We clear out and rebuild the list of surfaces that we can purge each
  // frame.
  to_purge_.clear();

  // Iterate through all nodes in |seen_| and remove the ones that were not seen
  // last frame, track the cached ones that we would like to persist in the
  // cache, reset the visited flags, and make a list of all nodes that we would
  // like to consider for caching next frame.
  std::vector<NodeData*> to_consider_for_cache;
  for (NodeMap::iterator iter = seen_.begin(); iter != seen_.end();) {
    NodeMap::iterator current = iter;
    ++iter;
    NodeData* node_data = &current->second;

    // We are re-determining which nodes are cache candidates now, so reset
    // the value determined last frame.
    node_data->is_cache_candidate = false;

    if (!node_data->visited_this_frame) {
      // If we did not visit this node last frame, remove it from the seen list.
      RemoveFromSeen(current);
    } else {
      if (IsCached(*node_data)) {
        if (MeetsCachePurgeCriteria(*node_data)) {
          // If a candidate no longer meets the requirements for staying
          // resident in the cache, mark it as removable and let it be freed
          // when we discover we need more space.
          to_purge_.push_back(node_data);
        } else {
          // Persist this existing cached node within the cache.
          candidate_cache_size += node_data->size_in_bytes;
          node_data->is_cache_candidate = true;
        }
      } else {
        // If the node is not cached by could be cached, at it to
        // |to_consider_for_cache| for consideration to be cached.
        if (MeetsCachingCriteria(*node_data)) {
          to_consider_for_cache.push_back(node_data);
        }
      }

      // Reset seen flags for the next frame.
      node_data->visited_this_frame = false;
      ++node_data->consecutive_frames_visited_;
    }
  }

  // Sort the list of items that are to be considered for caching ordered by
  // duration per pixel, so that we make the most of our limited cache space.
  std::sort(to_consider_for_cache.begin(), to_consider_for_cache.end(),
            SortByDurationPerPixel());

  // We now create a |to_add| list composed of the first nodes in
  // |to_consider_for_cache|, as they have been sorted to have the largest
  // durations per pixel.  Add them to the cache until we run out of space.
  std::vector<NodeData*> to_add;
  for (size_t i = 0; i < to_consider_for_cache.size(); ++i) {
    NodeData* node_data = to_consider_for_cache[i];
    DCHECK(!IsCached(*node_data));
    if (candidate_cache_size + node_data->size_in_bytes <= capacity_in_bytes_) {
      candidate_cache_size += node_data->size_in_bytes;
      to_add.push_back(node_data);
    }
  }

  // It is guaranteed that all items in |to_add| can fit into the cache, however
  // we are limited by how many items can be added to the cache per frame, so
  // of the items in |to_add|, we choose the items that have the largest total
  // duration as the ones we will add over the course of the next frame.
  std::sort(to_add.begin(), to_add.end(), SortByDuration());
  int num_to_add = std::min(to_add.size(), kMaxElementsToCachePerFrame);
  for (size_t i = 0; i < num_to_add; ++i) {
    // Finally mark the item as a cache candidate.
    to_add[i]->is_cache_candidate = true;
  }
}

bool SurfaceCache::MeetsCachingCriteria(const NodeData& node_data) const {
  const math::SizeF& bounds = node_data.render_size;

  // Only allow for caching nodes that take much longer to render than it takes
  // to apply a cached node, and also only allow caching nodes whose cached
  // surface is not larger than the maximum cached surface size.
  return bounds.width() > 0 && bounds.height() > 0 &&
         bounds.width() < (maximum_surface_size_.width() - 2) &&
         bounds.height() < (maximum_surface_size_.height() - 2) &&
         node_data.duration.InMicroseconds() >
             *average_apply_surface_time_in_usecs_.average() *
                 kApplySurfaceTimeMultipleCacheCriteria;
}

bool SurfaceCache::MeetsCachePurgeCriteria(const NodeData& node_data) const {
  // Returns true if a cached item should be removed from the cache.  When
  // comparing this function to MeetsCachingCriteria() above, it should be
  // easier for already cached items to stay in the cache in order to induce
  // stability in the cached nodes.
  return node_data.duration.InMicroseconds() <
         *average_apply_surface_time_in_usecs_.average() *
             kApplySurfaceTimeMultiplePurgeCriteria;
}

bool SurfaceCache::IsCached(const NodeData& node_data) const {
  return node_data.surface != NULL;
}

bool SurfaceCache::IsCacheCandidate(const NodeData& node_data) const {
  return node_data.is_cache_candidate;
}

void SurfaceCache::SetCachedSurface(NodeData* node_data,
                                    CachedSurface* surface) {
  DCHECK(!node_data->surface);
  DCHECK_LE(node_data->size_in_bytes, capacity_in_bytes_ - total_used_bytes_);

  node_data->surface = surface;
  total_used_bytes_ += node_data->size_in_bytes;
}

void SurfaceCache::FreeCachedSurface(NodeData* to_free) {
  TRACE_EVENT0("cobalt::renderer", "SurfaceCache::FreeCachedSurface()");
  DCHECK_LE(0, total_used_bytes_ - to_free->size_in_bytes);
  DCHECK(IsCached(*to_free));

  total_used_bytes_ -= to_free->size_in_bytes;

  delegate_->ReleaseSurface(to_free->surface);
  to_free->surface = NULL;

  DCHECK(!IsCached(*to_free));
}

void SurfaceCache::RemoveFromSeen(NodeMap::iterator to_remove) {
  DCHECK(to_remove != seen_.end());

  if (IsCached(to_remove->second)) {
    FreeCachedSurface(&to_remove->second);

    // Check the list of items to purge and remove it from there so that we
    // don't end up purging a stale NodeData.
    std::vector<NodeData*>::iterator found =
        std::find(to_purge_.begin(), to_purge_.end(), &to_remove->second);
    if (found != to_purge_.end()) {
      to_purge_.erase(found);
    }
  }
  seen_.erase(to_remove);
}

void SurfaceCache::PurgeUntilSpaceAvailable(size_t space_required_in_bytes) {
  while (capacity_in_bytes_ - total_used_bytes_ < space_required_in_bytes) {
    DCHECK(!to_purge_.empty())
        << "Frame() should have already ensured that if we need to purge, we "
        << "can purge.";

    NodeData* node_data_to_purge = to_purge_.back();
    to_purge_.pop_back();

    if (seen_.find(node_data_to_purge->node) != seen_.end()) {
      FreeCachedSurface(node_data_to_purge);
    }
  }
}

}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
