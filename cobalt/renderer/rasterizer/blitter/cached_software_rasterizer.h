// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_CACHED_SOFTWARE_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_CACHED_SOFTWARE_RASTERIZER_H_

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/c_val.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/renderer/rasterizer/blitter/render_state.h"
#include "cobalt/renderer/rasterizer/common/offscreen_render_coordinate_mapping.h"
#include "cobalt/renderer/rasterizer/skia/software_rasterizer.h"
#include "net/base/linked_hash_map.h"
#include "starboard/blitter.h"

#if SB_API_VERSION < 12 && SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

// This class is responsible for creating SbBlitterSurfaces and software
// rasterizing render tree nodes to them to be used by clients.  The class
// manages a cache of software surfaces in order to avoid duplicate work.  Its
// main public interface is for clients to construct
// CachedSoftwareRasterizer::SurfaceReference objects from which they can
// extract the software-rasterized surface.  Internally, software rasterization
// is handled via a skia::SoftwareRasterizer object.
class CachedSoftwareRasterizer {
 public:
  struct Surface {
    // The actual surface containing rendered data.
    SbBlitterSurface surface;

    // True if this surface was referenced at least once since the last time
    // OnStartNewFrame() was called.
    bool referenced;

#if defined(ENABLE_DEBUGGER)
    // True if OnStartNewFrame() has not been called since this surface was
    // created.
    bool created;
#endif  // defined(ENABLE_DEBUGGER)

    // Transform information detailing how the surface should be output to
    // the display such that sub-pixel alignments are respected.
    common::OffscreenRenderCoordinateMapping coord_mapping;

    // A duplicate of the key, though as a smart pointer, to keep a reference
    // to the node alive.
    scoped_refptr<render_tree::Node> node;

    // Is this surface cached or not.
    bool cached;

    // The scale used for rasterization when the surface was cached.
    math::Vector2dF scale;

    size_t GetEstimatedMemoryUsage() const;
  };

  CachedSoftwareRasterizer(SbBlitterDevice device, SbBlitterContext context,
                           int cache_capacity,
                           bool purge_skia_font_caches_on_destruction);
  ~CachedSoftwareRasterizer();

  // Should be called once per frame.  This method will remove from the cache
  // anything that hasn't been referenced in the last frame.
  void OnStartNewFrame();

  // A SurfaceReference is the mechanism through which clients can request
  // renders from the CachedSoftwareRasterizer.  The main reason that we don't
  // make GetSurface() the direct public interface is so that we can decide
  // to rasterize something without caching it, in which case it should be
  // cleaned up when the reference to it expires.
  class SurfaceReference {
   public:
    // The |transform| parameter should be the transform that the node will
    // ultimately have applied to it, and is used for ensuring that the rendered
    // result is sub-pixel aligned properly.
    SurfaceReference(CachedSoftwareRasterizer* rasterizer,
                     render_tree::Node* node, const Transform& transform)
        : surface_(rasterizer->GetSurface(node, transform)) {}
    ~SurfaceReference() {
      // If the surface returned to us was not actually cached, then destroy
      // it here when the surface reference is destroyed.
      if (!surface_.cached && SbBlitterIsSurfaceValid(surface_.surface)) {
        SbBlitterDestroySurface(surface_.surface);
      }
    }
    const Surface& surface() const { return surface_; }

   private:
    Surface surface_;

    DISALLOW_COPY_AND_ASSIGN(SurfaceReference);
  };

  Surface GetSurface(render_tree::Node* node, const Transform& transform);

  render_tree::ResourceProvider* GetResourceProvider() {
    return software_rasterizer_.GetResourceProvider();
  }

 private:
  typedef net::linked_hash_map<render_tree::Node*, Surface> CacheMap;

  // Release surfaces until we have |space_needed| free bytes in the cache.
  // This function will never release surfaces that were referenced this frame.
  // It is an error to call this function if it is impossible to purge
  // unreferenced surfaces until the desired amount of free space is available.
  void PurgeUntilSpaceAvailable(int space_needed);

  // The cache, mapping input render_tree::Node references to cached surfaces.
  CacheMap surface_map_;

  const int cache_capacity_;

  // The Blitter device/context that all image allocation/blitting operations
  // will occur within.
  SbBlitterDevice device_;
  SbBlitterContext context_;

  // The internal rasterizer used to actually render nodes.
  skia::SoftwareRasterizer software_rasterizer_;

  // The amount of memory currently consumed by the surfaces populating the
  // cache.
  base::CVal<base::cval::SizeInBytes> cache_memory_usage_;

  // Cache memory used this frame only.
  base::CVal<base::cval::SizeInBytes> cache_frame_usage_;
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION < 12 && SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_CACHED_SOFTWARE_RASTERIZER_H_
