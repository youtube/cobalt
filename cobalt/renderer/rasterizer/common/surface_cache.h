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

#ifndef COBALT_RENDERER_RASTERIZER_COMMON_SURFACE_CACHE_H_
#define COBALT_RENDERER_RASTERIZER_COMMON_SURFACE_CACHE_H_

#include <map>
#include <vector>

#include "base/hash_tables.h"
#include "base/time.h"
#include "cobalt/base/c_val.h"
#include "cobalt/math/exponential_moving_average.h"
#include "cobalt/math/size.h"
#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {

// A SurfaceCache can be owned and used by a rasterizer in order to cache
// surfaces representing sub-trees of a render tree as the rasterizer is
// visiting them.  The SurfaceCache will manage deciding which nodes to cache
// by measuring how long it takes to render each subtree.  The cache has
// a hard capacity limit given in bytes, and so the SurfaceCache will prioritize
// surfaces to cache by which ones have the highest time spent per pixel.
//
// Clients of SurfaceCache (e.g. rasterizers) should call SurfaceCache::Frame()
// once per frame.  When this is called, SurfaceCache will reset all "has been
// seen this frame" flags, marking any cached surfaces that were not seen as
// being purgeable.  It will also build a list of all cache candidates for the
// next frame.
//
// As the rasterizer visits each node of the render tree, it may insert lines
// like the following in order to use the cache:
//
//   SurfaceCache::Block cache_block(surface_cache, render_tree_node);
//   if (cache_block.Cached()) return;
//
// where |surface_cache| is a pointer to a SurfaceCache instance.  In this case,
// if the node is not already cached, a timer will be started to measure how
// long it takes to render.  If the node is not in the cache but it is decided
// that it should be based on a previous measurement, the SurfaceCache::Block
// constructor will activate the recording of the subtree and store the results
// into the cache upon the event that SurfaceCache::Block's destructor is
// called.  If the node is already in the cache, the cached version is rendered
// immediately by SurfaceCache::Block's constructor, and then
// cache_block.Cached() will return true.

class SurfaceCache {
 public:
  // An opaque type representing a cached surface.  These objects are intended
  // to be created and destroyed by SurfaceCache::Delegates.
  class CachedSurface {
   public:
    virtual ~CachedSurface() {}
  };

  // Implementations of SurfaceCache::Delegate are responsible for providing all
  // of the support for how to create/destroy CachedSurface objects, how to
  // setup the backend rasterizer when it is time to record to the cache, as
  // well as how to playback a cached surface to a render target.
  class Delegate {
   public:
    // Playback a cached surface.  This should exactly reproduce whatever
    // was recorded within previous calls to StartRecording()/EndRecording().
    virtual void ApplySurface(CachedSurface* surface) = 0;

    // Called to instruct the Delegate to start recording all rasterization
    // calls within the specified local bounding rectangle (e.g. the results
    // from calling a render tree node's GetBounds() method).
    virtual void StartRecording(const math::RectF& local_bounds) = 0;

    // Called to instruct the Delegate to stop recording and return a
    // CachedSurface that can be used to playback (via ApplySurface()) the the
    // rendering calls made since StartRecording() was called.
    virtual CachedSurface* EndRecording() = 0;

    // Called to instruct the Delegate that a surface is no longer needed and
    // can be freed.
    virtual void ReleaseSurface(CachedSurface* surface) = 0;

    // Called to query the size a render surface will occupy given its local
    // size.  This will, for example, take into account any scaling that is
    // induced by the node's ancestors.
    virtual math::Size GetRenderSize(const math::SizeF& local_size) = 0;

    // Returns the maximum surface size that can be cached.
    virtual math::Size MaximumSurfaceSize() = 0;
  };

 private:
  // NodeData contains a set of data stored for every node that we have seen
  // and helps determine whether that node should be cached or not, or if it
  // is already cached.
  struct NodeData {
    explicit NodeData(render_tree::Node* node)
        : microseconds_per_pixel(-1.0f),
          size_in_bytes(-1),
          surface(NULL),
          visited_this_frame(true),
          consecutive_frames_visited_(0),
          is_cache_candidate(false),
          node(node) {}

    // Updates |microseconds_per_pixel| based on the values for |duration| and
    // |render_size|.
    void UpdateMicrosecondsPerPixel();
    // Updates |size_in_bytes| based on the values for |render_size|.
    void UpdateSizeInBytes();

    // Tracks how long it last took to render this node.
    base::TimeDelta duration;
    // Tracks the render size of this node, which may be different than
    // node->GetBounds() because of ancestor node transformations.
    math::Size render_size;
    // How many microseconds are spent per pixel of this node, on average.
    float microseconds_per_pixel;
    // The size in bytes occupied by a cached surface representing this node.
    int size_in_bytes;

    // If the surface is cached, this will point to the CachedSurface object
    // returned by the Delegate.  Otherwise, this will be null.
    CachedSurface* surface;

    // Was this node visited since SurfaceCache::Frame() was last called?
    bool visited_this_frame;

    // How many frames was this node visited consecutively?
    int consecutive_frames_visited_;

    // True if this node should be inserted into the cache next time it is
    // encountered.
    bool is_cache_candidate;

    // A reference to the actual node.
    scoped_refptr<render_tree::Node> node;
  };
  typedef base::hash_map<render_tree::Node*, NodeData> NodeMap;

 public:
  // The main public interface to SurfaceCache is via SurfaceCache::Block.
  // A block's lifetime should surround all rendering calls needed to render
  // a render tree node.  Blocks are responsible for measuring node render
  // times, starting/stopping node recordings, and playing back cached nodes.
  // Most of the logic lives in Block's constructor and destructor.
  class Block {
   public:
    Block(SurfaceCache* surface_cache, render_tree::Node* node) {
      // We like this to be inlined so that this call makes very little
      // performance impact if |surface_cache| is null.
      cache_ = surface_cache;
      node_ = node;
      if (!cache_) {
        // If there is no cache, we have nothing to do besides indicate that we
        // are not recording and the surface for this node is not already
        // cached.
        state_ = kStateNotRecording;
        return;
      } else {
        InitBlock();
      }
    }
    ~Block() {
      // We like this to be inlined so that this call makes very little
      // performance impact if |cache_| is null.
      if (cache_) {
        ShutdownBlock();
      }
    }

    // Returns true if the block represents a cached node.  This should be
    // called by clients to return early since Cached() returning true implies
    // that Block::Block() has already applied the cached surface.
    bool Cached() const { return state_ == kStateAlreadyCached; }

   private:
    enum State {
      kStateAlreadyCached,
      kStateRecording,
      kStateNotRecording,
      kStateInvalid
    };
    void InitBlock();
    void ShutdownBlock();

    // If any blocks on the stack are performing any timing, this method
    // iterates through them all and increases the duration.  This is done to
    // simulate actual render times of cached sub trees when simply playing back
    // the cached surface.
    void IncreaseAncestorBlockDurations(const base::TimeDelta& duration);

    State state_;

    SurfaceCache* cache_;
    render_tree::Node* node_;

    base::TimeTicks start_time_;
    NodeData* node_data_;

    Block* parent_;

    friend class SurfaceCache;
  };

  // The Delegate is responsible for handling surface management.
  // |capcity_in_bytes| specifies the hard capacity of the surface cache.
  SurfaceCache(Delegate* delegate, size_t capacity_in_bytes);

  // This should be called by clients (e.g. rasterizers) once per frame, to
  // reset the "seen" flags of the cache and determine what is eligible for
  // caching in subsequent frames.
  void Frame();

 private:
  class SortByDurationPerPixel;
  class SortByDuration;

  // Returns true if it is possible to cache the specified node.
  bool MeetsCachingCriteria(const NodeData& node_data) const;
  // Returns true if the node should be purged from the cache despite the fact
  // that it continues to be seen.
  bool MeetsCachePurgeCriteria(const NodeData& node_data) const;

  // Returns true if the specified node is in the cache.
  bool IsCached(const NodeData& node_data) const;
  // Returns true if the specified node is marked for being cachable.
  bool IsCacheCandidate(const NodeData& node_data) const;

  // Attaches a cached surface produced by the delegate to a NodeData object.
  void SetCachedSurface(NodeData* node_data, CachedSurface* surface);
  // Releases a cached surface belonging to the specified NodeData object.
  void FreeCachedSurface(NodeData* to_free);

  // Removes a node from the list of seen nodes.
  void RemoveFromSeen(NodeMap::iterator to_remove);

  // Removes cached nodes in |purge_candidates_| until enough space is
  // free.
  void PurgeUntilSpaceAvailable(size_t space_required_in_bytes);

  Delegate* delegate_;

  // A list of all nodes that have been seen since the last frame started.
  NodeMap seen_;

  // Number of bytes occupied by cached surface pixel data.
  base::DebugCVal<int> total_used_bytes_;

  // Maximum number of bytes that can be occupied by cached surface pixel data.
  int capacity_in_bytes_;

  // True if we are currently recording to cache.
  bool recording_;

  // A link to the currently active block, so that when a new block is created,
  // its parent pointer can be established.
  Block* current_block_;

  // The average duration for calls to Delegate::ApplySurface().  This is useful
  // when determining if it is worth it or not to cache a render tree.
  math::ExponentialMovingAverage<int64> average_apply_surface_time_in_usecs_;

  // A locally saved value of the maximum surface size, as given by
  // Delegate::MaximumSurfaceSize().
  math::Size maximum_surface_size_;

  std::vector<NodeData*> to_purge_;

  friend class Block;
};

}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_COMMON_SURFACE_CACHE_H_
