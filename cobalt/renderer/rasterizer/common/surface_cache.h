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
#include "cobalt/base/c_val_time_interval_entry_stats.h"
#include "cobalt/math/size.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/renderer/rasterizer/common/streaming_best_fit_line.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {

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
  // The key to our cache's map of which items it has seen before.  We use
  // the address of a render node along with its on-screen render size to
  // determine whether there is a cache match or not.
  struct NodeMapKey {
    NodeMapKey() {}
    NodeMapKey(render_tree::Node* node, const math::Size& render_size)
        : node(node), render_size(render_size) {}

    bool operator<(const NodeMapKey& rhs) const {
      return (node == rhs.node
                  ? (render_size.width() == rhs.render_size.width()
                         ? render_size.height() < rhs.render_size.height()
                         : render_size.width() < rhs.render_size.width())
                  : node < rhs.node);
    }

    render_tree::Node* node;
    math::Size render_size;
  };
  // NodeData contains a set of data stored for every node that we have seen
  // and helps determine whether that node should be cached or not, or if it
  // is already cached.
  struct NodeData {
    explicit NodeData(const NodeMapKey& key)
        : render_size(key.render_size),
          surface(NULL),
          visited_this_frame(true),
          consecutive_frames_visited(0),
          is_cache_candidate(false),
          node(key.node) {}

    // The size in bytes occupied by a cached surface representing this node.
    int size_in_bytes() { return render_size.GetArea() * 4; }

    // Returns whether or not this node is cached or not.
    bool cached() const { return surface != NULL; }

    // Creates a key from the NodeData.
    NodeMapKey key() const { return NodeMapKey(node.get(), render_size); }

    // Tracks how long it last took to render this node.
    base::TimeDelta duration;

    // Tracks the render size of this node, which may be different than
    // node->GetBounds() because of ancestor node transformations.
    math::Size render_size;

    // If the surface is cached, this will point to the CachedSurface object
    // returned by the Delegate.  Otherwise, this will be null.
    CachedSurface* surface;

    // Was this node visited since SurfaceCache::Frame() was last called?
    bool visited_this_frame;

    // How many frames was this node visited consecutively?
    int consecutive_frames_visited;

    // True if this node should be inserted into the cache next time it is
    // encountered.
    bool is_cache_candidate;

    // A reference to the actual node.
    scoped_refptr<render_tree::Node> node;
  };
  typedef std::map<NodeMapKey, NodeData> NodeMap;

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
      if (!cache_) {
        // If there is no cache, we have nothing to do besides indicate that we
        // are not recording and the surface for this node is not already
        // cached.
        state_ = kStateNotRecording;
        return;
      } else {
        InitBlock(node);
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
    void InitBlock(render_tree::Node* node);
    void ShutdownBlock();

    // If any blocks on the stack are performing any timing, this method
    // iterates through them all and increases the duration.  This is done to
    // simulate actual render times of cached sub trees when simply playing back
    // the cached surface.
    void IncreaseAncestorBlockDurations(const base::TimeDelta& duration);

    State state_;

    SurfaceCache* cache_;
    NodeMapKey key_;

    base::TimeTicks start_time_;
    NodeData* node_data_;

    Block* parent_;

    friend class SurfaceCache;
  };

  // The Delegate is responsible for handling surface management.
  // |capcity_in_bytes| specifies the hard capacity of the surface cache.
  SurfaceCache(Delegate* delegate, size_t capacity_in_bytes);
  ~SurfaceCache();

  // This should be called by clients (e.g. rasterizers) once per frame, to
  // reset the "seen" flags of the cache and determine what is eligible for
  // caching in subsequent frames.
  void Frame();

 private:
  class SortByDurationDifferencePerPixel;
  class SortByDurationDifference;

  // Helper function to determine which nodes within |nodes| would be best
  // to cache, given that they fit within |cache_capacity|.  This method takes
  // ownership of |nodes| and modifies it, so its contents may not remain intact
  // after this call.  |apply_surface_time_model| is the model of how much time
  // it takes to render a cached surface.  The resulting set of nodes that
  // should be cached will be placed in the output parameter, |results|.  The
  // return value is the total size of all cached nodes, and is guaranteed to
  // be less than |cache_capacity|.
  static int SelectNodesToCacheNextFrame(const Line& apply_surface_time_model,
                                         int cache_capacity,
                                         std::vector<NodeData*>* nodes,
                                         std::vector<NodeData*>* results);

  // Returns true if it is possible to cache the specified node.
  bool MeetsCachingCriteria(const NodeData& node_data,
                            const Line& apply_surface_time_model) const;
  // Returns true if the node should be purged from the cache despite the fact
  // that it continues to be seen.
  bool MeetsCachePurgeCriteria(const NodeData& node_data,
                               const Line& apply_surface_time_model) const;

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
  int total_used_bytes_;

  // CVals for introspection into cache details.
  base::CValTimeIntervalEntryStats<int> total_used_bytes_cval_;
  base::CValTimeIntervalEntryStats<float> apply_surface_duration_overhead_cval_;
  base::CValTimeIntervalEntryStats<float>
      apply_surface_duration_per_pixel_cval_;
  int cached_surfaces_count_;
  base::CValTimeIntervalEntryStats<float> cached_surfaces_count_cval_;
  int surfaces_freed_this_frame_;
  base::CValTimeIntervalEntryStats<float> surfaces_freed_per_frame_cval_;

  // Maximum number of bytes that can be occupied by cached surface pixel data.
  int capacity_in_bytes_;

  // True if we are currently recording to cache.
  bool recording_;

  // A link to the currently active block, so that when a new block is created,
  // its parent pointer can be established.
  Block* current_block_;

  // The average duration for calls to Delegate::ApplySurface().  This is useful
  // when determining if it is worth it or not to cache a render tree.
  StreamingBestFitLine apply_surface_time_regressor_;

  // A locally saved value of the maximum surface size, as given by
  // Delegate::MaximumSurfaceSize().
  math::Size maximum_surface_size_;

  std::vector<NodeData*> to_purge_;

  // Used temporarily in the scope of Frame(), but stored as a member to avoid
  // reallocating storage for it.
  std::vector<NodeData*> to_consider_for_cache_;
  std::vector<NodeData*> to_add_;

  friend class Block;
};

}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_COMMON_SURFACE_CACHE_H_
