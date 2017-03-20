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

#include "cobalt/renderer/rasterizer/common/surface_cache.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/debug/trace_event.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {

namespace {
// The fraction of how much each newly measured apply surface duration affects
// the current exponentially-weighted average apply surface duration.
const float kApplySurfaceDurationChangeRate = 0.01f;

// The initial guess for the duration of apply surface.  This will be updated
// as we acquire data, but until then we start with this guess.
const int64 kInitialApplySurfaceTimeInUSecs = 100;

// Since caching elements is expensive, we limit the number of new entries added
// to the cache each frame.
const size_t kMaxElementsToCachePerFrame = 1;

// If a node has been seen for kConsecutiveFramesForPreference consecutive
// frames, it is preferred for being cached over nodes that have been seen less
// consecutive times.  A setting of 2 is qualitatively important because it is
// impossible for an animated node to reach a value of 2 or more, and so by
// setting this value to 2, we will always prefer non-animated nodes over
// animated nodes.
const int kConsecutiveFramesForPreference = 2;

// A surface must take longer than the duration of applying a cached surface
// in order for it to be considered for caching.  In particular, it must take
// kApplySurfaceTimeMultipleCacheCriteria times as long, to put a gap between
// criteria for accepting a surface into cache and criteria for purging from
// cache.
const float kApplySurfaceTimeMultipleCacheCriteria = 1.2f;
}  // namespace

void SurfaceCache::Block::InitBlock(render_tree::Node* node) {
  start_time_ = base::TimeTicks::Now();

  key_ = NodeMapKey(node,
                    cache_->delegate_->GetRenderSize(node->GetBounds().size()));

  // Keep track of the stack of blocks currently being rendered.
  parent_ = cache_->current_block_;
  cache_->current_block_ = this;

  NodeMap::iterator seen_iter = cache_->seen_.find(key_);
  node_data_ = (seen_iter == cache_->seen_.end() ? NULL : &seen_iter->second);

  if (node_data_) {
    // In order for the cache to function, it needs to know which nodes are
    // seen frequently and which are not.  Setting this flag is the main signal.
    node_data_->visited_this_frame = true;
  }

  if (node_data_ && node_data_->cached()) {
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
    // another block.
    base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
    cache_->apply_surface_time_regressor_.AddValue(
        node_data_->render_size.GetArea(),
        static_cast<float>(duration.InMicroseconds()));

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
      cache_->PurgeUntilSpaceAvailable(node_data_->size_in_bytes());

      // Signal to the delegate that we would like to record this block for
      // caching.
      cache_->delegate_->StartRecording(key_.node->GetBounds());
    } else {
      // The node is not cached, and it should not become cached, so just do
      // nothing besides record how long it takes via |start_time_|, set above.
      state_ = kStateNotRecording;
    }
  }
}

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
      if (key_.render_size.GetArea() == 0) {
        // Do not consider degenerate render tree nodes for the cache.
        break;
      }

      // We're done now, get metrics from the delegate and determine whether or
      // not we should cache the result for next time.
      base::TimeDelta duration = base::TimeTicks::Now() - start_time_;

      // Record that we've seen this block before, if we haven't already done
      // so.
      if (!node_data_) {
        std::pair<NodeMap::iterator, bool> insert_results =
            cache_->seen_.insert(std::make_pair(key_, NodeData(key_)));
        DCHECK(insert_results.second);
        node_data_ = &insert_results.first->second;
      }

      // Update the recorded entry with new timing information.
      node_data_->duration = duration;
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

namespace {
const uint64 kRefreshCValsIntervalInMS = 1000;
}  // namespace

SurfaceCache::SurfaceCache(Delegate* delegate, size_t capacity_in_bytes)
    : delegate_(delegate),
      total_used_bytes_(0),
      total_used_bytes_cval_("Renderer.SurfaceCache.BytesUsed",
                             kRefreshCValsIntervalInMS),
      apply_surface_duration_overhead_cval_(
          "Renderer.SurfaceCache.ApplySurfaceDurationOverhead",
          kRefreshCValsIntervalInMS),
      apply_surface_duration_per_pixel_cval_(
          "Renderer.SurfaceCache.ApplySurfaceDurationPerPixel",
          kRefreshCValsIntervalInMS),
      cached_surfaces_count_(0),
      cached_surfaces_count_cval_("Renderer.SurfaceCache.CachedSurfacesCount",
                                  kRefreshCValsIntervalInMS),
      surfaces_freed_this_frame_(0),
      surfaces_freed_per_frame_cval_(
          "Renderer.SurfaceCache.SurfacesFreedPerFrame",
          kRefreshCValsIntervalInMS),
      capacity_in_bytes_(capacity_in_bytes),
      recording_(false),
      current_block_(NULL),
      apply_surface_time_regressor_(kApplySurfaceDurationChangeRate, true,
                                    true),
      maximum_surface_size_(delegate_->MaximumSurfaceSize()) {
  // It will be updated while the application runs, but for initialization we
  // give the apply surface time regressor two points so that its model is
  // initialized to a constant line.
  apply_surface_time_regressor_.AddValue(0, kInitialApplySurfaceTimeInUSecs);
  apply_surface_time_regressor_.AddValue(1, kInitialApplySurfaceTimeInUSecs);
}

SurfaceCache::~SurfaceCache() {
  while (!seen_.empty()) {
    RemoveFromSeen(seen_.begin());
  }
}

// Sort NodeData objects in descending order by microseconds per pixel
// difference between actually drawing the node and applying a cached surface
// of the same size.  Prefer nodes that have been seen multiple frames in a row
// to appear before those that haven't.
class SurfaceCache::SortByDurationDifferencePerPixel {
 public:
  explicit SortByDurationDifferencePerPixel(
      const Line& apply_surface_duration_model)
      : apply_surface_duration_model_(apply_surface_duration_model) {}

  bool operator()(const NodeData* lhs, const NodeData* rhs) const {
    float lhs_area = lhs->render_size.GetArea();
    float rhs_area = rhs->render_size.GetArea();

    return (lhs->duration.InMicroseconds() -
            apply_surface_duration_model_.value_at(lhs_area)) /
               lhs_area >
           (rhs->duration.InMicroseconds() -
            apply_surface_duration_model_.value_at(rhs_area)) /
               rhs_area;
  }

 private:
  const Line apply_surface_duration_model_;
};

// Sort NodeData objects in descending order by the difference in duration to
// render between the actual node and applying a cached surface of the same
// size.  Prefer nodes that have been seen multiple frames in a row to appear
// before those that haven't.
class SurfaceCache::SortByDurationDifference {
 public:
  explicit SortByDurationDifference(const Line& apply_surface_duration_model)
      : apply_surface_duration_model_(apply_surface_duration_model) {}

  bool operator()(const NodeData* lhs, const NodeData* rhs) const {
    return lhs->duration.InMicroseconds() -
               static_cast<int64>(apply_surface_duration_model_.value_at(
                   lhs->render_size.GetArea())) >
           rhs->duration.InMicroseconds() -
               static_cast<int64>(apply_surface_duration_model_.value_at(
                   rhs->render_size.GetArea()));
  }

 private:
  const Line apply_surface_duration_model_;
};

void SurfaceCache::Frame() {
  TRACE_EVENT0("cobalt::renderer", "SurfaceCache::Frame()");

  Line apply_surface_time_model(apply_surface_time_regressor_.best_estimate());

  // Update CVals.
  total_used_bytes_cval_.AddEntry(total_used_bytes_);
  apply_surface_duration_overhead_cval_.AddEntry(
      apply_surface_time_model.y_intercept());
  apply_surface_duration_per_pixel_cval_.AddEntry(
      apply_surface_time_model.slope());
  cached_surfaces_count_cval_.AddEntry(cached_surfaces_count_);
  surfaces_freed_per_frame_cval_.AddEntry(surfaces_freed_this_frame_);
  surfaces_freed_this_frame_ = 0;

  // We are about to determine what will potentially appear in the cache for
  // the next frame.  |candidate_cache_size| will keep track of how many bytes
  // will be in cache.
  int candidate_cache_size = 0;

  {
    TRACE_EVENT0("cobalt::renderer", "Calculate to_consider_for_cache_");

    // We clear out and rebuild the list of surfaces that we can purge each
    // frame.
    to_purge_.clear();

    // Iterate through all nodes in |seen_| and remove the ones that were not
    // seen last frame, track the cached ones that we would like to persist in
    // the cache, reset the visited flags, and make a list of all nodes that we
    // would like to consider for caching next frame.
    to_consider_for_cache_.clear();
    for (NodeMap::iterator iter = seen_.begin(); iter != seen_.end();) {
      NodeMap::iterator current = iter;
      ++iter;
      NodeData* node_data = &current->second;

      // We are re-determining which nodes are cache candidates now, so reset
      // the value determined last frame.
      node_data->is_cache_candidate = false;
      ++node_data->consecutive_frames_visited;

      if (!node_data->visited_this_frame) {
        // If we did not visit this node last frame, remove it from the seen
        // list.
        RemoveFromSeen(current);
      } else {
        if (node_data->cached()) {
          if (MeetsCachePurgeCriteria(*node_data, apply_surface_time_model)) {
            // If a candidate no longer meets the requirements for staying
            // resident in the cache, mark it as removable and let it be freed
            // when we discover we need more space.
            to_purge_.push_back(node_data);
          } else {
            // Persist this existing cached node within the cache.
            candidate_cache_size += node_data->size_in_bytes();
            node_data->is_cache_candidate = true;
          }
        } else {
          // If the node is not cached but could be cached, at it to
          // |to_consider_for_cache_| for consideration to be cached.
          if (MeetsCachingCriteria(*node_data, apply_surface_time_model)) {
            to_consider_for_cache_.push_back(node_data);
          }
        }

        // Reset seen flags for the next frame.
        node_data->visited_this_frame = false;
      }
    }
  }

  {
    TRACE_EVENT0("cobalt::renderer", "Sort to_purge_");
    // Sort |to_purge_| so that items that are the least beneficial to keep in
    // the cache appear at the end of the list, and so get purged first.
    std::sort(to_purge_.begin(), to_purge_.end(),
              SortByDurationDifference(apply_surface_time_model));
  }

  // We now create a |to_add_| list composed of the first nodes in
  // |to_consider_for_cache_| after it is sorted to have the largest
  // durations per pixel.  These are added to |to_add| until we run out of
  // cache capacity.
  candidate_cache_size += SelectNodesToCacheNextFrame(
      apply_surface_time_model, capacity_in_bytes_ - candidate_cache_size,
      &to_consider_for_cache_, &to_add_);

  {
    TRACE_EVENT0("cobalt::renderer", "Calculate cache candidates");

    // It is guaranteed that all items in |to_add_| can fit into the cache,
    // however we are limited by how many items can be added to the cache per
    // frame, so of the items in |to_add_|, we choose the items that have the
    // largest total duration as the ones we will add over the course of the
    // next frame.
    if (to_add_.size() > kMaxElementsToCachePerFrame) {
      std::sort(to_add_.begin(), to_add_.end(),
                SortByDurationDifference(apply_surface_time_model));
    }
    int num_to_add = std::min(to_add_.size(), kMaxElementsToCachePerFrame);
    for (size_t i = 0; i < num_to_add; ++i) {
      // Finally mark the item as a cache candidate.
      to_add_[i]->is_cache_candidate = true;
    }
  }
}

int SurfaceCache::SelectNodesToCacheNextFrame(
    const Line& apply_surface_time_model, int cache_capacity,
    std::vector<NodeData*>* nodes, std::vector<NodeData*>* results) {
  TRACE_EVENT0("cobalt::renderer",
               "SurfaceCache::SelectNodesToCacheNextFrame()");

  int candidate_cache_size = 0;

  results->clear();

  // Sort the list of items that are to be considered for caching ordered by
  // duration per pixel, so that we make the most of our limited cache space.
  std::sort(nodes->begin(), nodes->end(),
            SortByDurationDifferencePerPixel(apply_surface_time_model));

  // Iterate through each node in our sorted set of cacheable nodes and
  // determine if we can host them in our cache or not.
  for (size_t i = 0; i < nodes->size(); ++i) {
    NodeData* node_data = (*nodes)[i];
    DCHECK(!node_data->cached());

    // For each node to consider, we add it to |results| if there is available
    // cache capacity remaining.  If there is not, we walk backwards through our
    // list of already accepted nodes (which are sorted by duration difference
    // per pixel) and add up the duration difference (difference between
    // drawing the node and rendering a cached surface of the same size).  As
    // long as the duration difference gained by caching the current node
    // exceeds the sum of duration differences that we are replacing, it would
    // be better to perform that replacement.
    int size_in_bytes = node_data->size_in_bytes();
    base::TimeDelta duration_difference =
        node_data->duration -
        base::TimeDelta::FromMicroseconds(apply_surface_time_model.value_at(
            node_data->render_size.GetArea()));

    int replace_index = static_cast<int>(results->size());
    int replace_size = 0;
    base::TimeDelta replace_duration;
    while (true) {
      // Check if the node fits in the cache.
      if (candidate_cache_size + size_in_bytes - replace_size <=
          cache_capacity) {
        // Adjust the cache size given the nodes we will remove from the cache
        // to replace with this new node.
        candidate_cache_size -= replace_size;
        candidate_cache_size += size_in_bytes;
        // Remove the replaced nodes (we may replace no nodes).
        results->erase(results->begin() + replace_index, results->end());
        // And add our new node.
        results->push_back(node_data);
        break;
      }

      // We were not able to fit in the cache, so see if we would be better
      // off to remove one more item from the cache to be replaced with this
      // node.
      --replace_index;

      // If we've run out of nodes to replace and there's still no space for
      // this one, then this node simply won't fit in the cache and we're done.
      if (replace_index < 0) {
        break;
      }

      // Increase the sum of the duration differences that we are replacing and
      // test that it's still less than the duration difference we would obtain
      // by adding the new node.  If it is not still less, then it would not
      // be better to replace those nodes with this one, so we're done.
      replace_duration +=
          (*results)[replace_index]->duration -
          base::TimeDelta::FromMicroseconds(apply_surface_time_model.value_at(
              (*results)[replace_index]->render_size.GetArea()));
      if (replace_duration > duration_difference) {
        break;
      }
      replace_size += (*results)[replace_index]->size_in_bytes();
    }
  }

  return candidate_cache_size;
}

bool SurfaceCache::MeetsCachingCriteria(
    const NodeData& node_data, const Line& apply_surface_time_model) const {
  const math::SizeF& bounds = node_data.render_size;

  // Only allow for caching nodes that take much longer to render than it takes
  // to apply a cached node, and also only allow caching nodes whose cached
  // surface is not larger than the maximum cached surface size.
  return node_data.consecutive_frames_visited >=
             kConsecutiveFramesForPreference &&
         bounds.width() > 0 && bounds.height() > 0 &&
         bounds.width() < (maximum_surface_size_.width() - 2) &&
         bounds.height() < (maximum_surface_size_.height() - 2) &&
         node_data.duration.InMicroseconds() >
             apply_surface_time_model.value_at(
                 node_data.render_size.GetArea()) *
                 kApplySurfaceTimeMultipleCacheCriteria;
}

bool SurfaceCache::MeetsCachePurgeCriteria(
    const NodeData& node_data, const Line& apply_surface_time_model) const {
  // Returns true if a cached item should be removed from the cache.  When
  // comparing this function to MeetsCachingCriteria() above, it should be
  // easier for already cached items to stay in the cache in order to induce
  // stability in the cached nodes.
  return node_data.duration.InMicroseconds() <
         apply_surface_time_model.value_at(node_data.render_size.GetArea());
}

bool SurfaceCache::IsCacheCandidate(const NodeData& node_data) const {
  return node_data.is_cache_candidate;
}

void SurfaceCache::SetCachedSurface(NodeData* node_data,
                                    CachedSurface* surface) {
  DCHECK(!node_data->surface);
  DCHECK_LE(node_data->size_in_bytes(), capacity_in_bytes_ - total_used_bytes_);

  ++cached_surfaces_count_;
  node_data->surface = surface;
  total_used_bytes_ += node_data->size_in_bytes();
}

void SurfaceCache::FreeCachedSurface(NodeData* to_free) {
  TRACE_EVENT0("cobalt::renderer", "SurfaceCache::FreeCachedSurface()");
  DCHECK_LE(0, total_used_bytes_ - to_free->size_in_bytes());
  DCHECK(to_free->cached());

  total_used_bytes_ -= to_free->size_in_bytes();

  delegate_->ReleaseSurface(to_free->surface);
  to_free->surface = NULL;

  ++surfaces_freed_this_frame_;
  --cached_surfaces_count_;

  DCHECK(!to_free->cached());
}

void SurfaceCache::RemoveFromSeen(NodeMap::iterator to_remove) {
  DCHECK(to_remove != seen_.end());

  if (to_remove->second.cached()) {
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

    if (seen_.find(node_data_to_purge->key()) != seen_.end()) {
      FreeCachedSurface(node_data_to_purge);
    }
  }
}

}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
