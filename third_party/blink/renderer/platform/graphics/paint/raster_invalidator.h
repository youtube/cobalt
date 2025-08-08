// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_RASTER_INVALIDATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_RASTER_INVALIDATOR_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "third_party/blink/renderer/platform/graphics/compositing/chunk_to_layer_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

class PaintArtifact;

class PLATFORM_EXPORT RasterInvalidator {
  USING_FAST_MALLOC(RasterInvalidator);

 public:
  using RasterInvalidationFunction =
      base::RepeatingCallback<void(const gfx::Rect&)>;

  RasterInvalidator() = default;

  void SetTracksRasterInvalidations(bool);
  RasterInvalidationTracking* GetTracking() const { return tracking_.get(); }

  RasterInvalidationTracking& EnsureTracking();

  // Generate raster invalidations for a subset of the paint chunks in the
  // paint artifact.
  void Generate(RasterInvalidationFunction,
                const PaintChunkSubset&,
                const gfx::Vector2dF& layer_offset,
                const gfx::Size& layer_bounds,
                const PropertyTreeState& layer_state);

  // Called when we repainted PaintArtifact but a ContentLayerClientImpl doesn't
  // have anything changed. We just need to let |old_paint_artifact_| point to
  // the real old one. TODO(wangxianzhu): When we remove pre-CAP code, we can
  // avoid this function by storing the old paint artifact in
  // PaintArtifactCompositor and pass it in Generate().
  void SetOldPaintArtifact(scoped_refptr<const PaintArtifact>);

  const gfx::Size& LayerBounds() const { return layer_bounds_; }

  size_t ApproximateUnsharedMemoryUsage() const;

  void ClearOldStates();

 private:
  friend class DisplayItemRasterInvalidator;
  friend class RasterInvalidatorTest;

  struct PaintChunkInfo {
    DISALLOW_NEW();

   public:
    PaintChunkInfo(const RasterInvalidator& invalidator,
                   const ChunkToLayerMapper& mapper,
                   const PaintChunkIterator& chunk_it)
        : index_in_paint_artifact(chunk_it.IndexInPaintArtifact()),
#if DCHECK_IS_ON()
          id(chunk_it->id),
#endif
          bounds_in_layer(invalidator.ClipByLayerBounds(
              mapper.MapVisualRect(chunk_it->drawable_bounds))),
          chunk_to_layer_clip(mapper.ClipRect()),
          chunk_to_layer_transform(mapper.Transform()) {
    }

    PaintChunkInfo(const PaintChunkInfo& old_chunk_info,
                   const PaintChunkIterator& chunk_it)
        : index_in_paint_artifact(chunk_it.IndexInPaintArtifact()),
#if DCHECK_IS_ON()
          id(chunk_it->id),
#endif
          bounds_in_layer(old_chunk_info.bounds_in_layer),
          chunk_to_layer_clip(old_chunk_info.chunk_to_layer_clip),
          chunk_to_layer_transform(old_chunk_info.chunk_to_layer_transform) {
#if DCHECK_IS_ON()
      DCHECK_EQ(id, old_chunk_info.id);
#endif
    }

    // The index of the chunk in the PaintArtifact. It may be different from
    // the index of this PaintChunkInfo in paint_chunks_info_ when a subset of
    // the paint chunks is handled by the RasterInvalidator.
    wtf_size_t index_in_paint_artifact;

#if DCHECK_IS_ON()
    PaintChunk::Id id;
#endif

    gfx::Rect bounds_in_layer;
    FloatClipRect chunk_to_layer_clip;
    gfx::Transform chunk_to_layer_transform;
  };

  void GenerateRasterInvalidations(RasterInvalidationFunction,
                                   const PaintChunkSubset&,
                                   bool layer_offset_or_state_changed,
                                   bool layer_effect_changed,
                                   Vector<PaintChunkInfo>& new_chunks_info);

  ALWAYS_INLINE const PaintChunk& GetOldChunk(wtf_size_t index) const;
  ALWAYS_INLINE wtf_size_t MatchNewChunkToOldChunk(const PaintChunk& new_chunk,
                                                   wtf_size_t old_index) const;

  ALWAYS_INLINE void IncrementallyInvalidateChunk(
      RasterInvalidationFunction,
      const PaintChunkInfo& old_chunk_info,
      const PaintChunkInfo& new_chunk_info,
      DisplayItemClientId);

  // |old_or_new| indicates whether |client| is from the old or new
  // PaintArtifact, so we know which one can provide the client's debug name.
  enum ClientIsOldOrNew { kClientIsOld, kClientIsNew };
  void AddRasterInvalidation(RasterInvalidationFunction function,
                             const gfx::Rect& rect,
                             DisplayItemClientId client_id,
                             PaintInvalidationReason reason,
                             ClientIsOldOrNew old_or_new) {
    if (rect.IsEmpty())
      return;
    function.Run(rect);
    if (tracking_)
      TrackRasterInvalidation(rect, client_id, reason, old_or_new);
  }
  void TrackRasterInvalidation(const gfx::Rect&,
                               DisplayItemClientId,
                               PaintInvalidationReason,
                               ClientIsOldOrNew);

  ALWAYS_INLINE PaintInvalidationReason
  ChunkPropertiesChanged(const PaintChunk& new_chunk,
                         const PaintChunk& old_chunk,
                         const PaintChunkInfo& new_chunk_info,
                         const PaintChunkInfo& old_chunk_info,
                         const PropertyTreeState& layer_state,
                         const float absolute_translation_tolerance,
                         const float other_transform_tolerance) const;

  // Clip a rect in the layer space by the layer bounds.
  template <typename Rect>
  Rect ClipByLayerBounds(const Rect& r) const {
    return IntersectRects(r, Rect(gfx::Rect(layer_bounds_)));
  }

  gfx::Vector2dF layer_offset_;
  gfx::Size layer_bounds_;
  PropertyTreeState layer_state_ = PropertyTreeState::Root();
  Vector<PaintChunkInfo> old_paint_chunks_info_;
  scoped_refptr<const PaintArtifact> current_paint_artifact_;
  scoped_refptr<const PaintArtifact> old_paint_artifact_;

  std::unique_ptr<RasterInvalidationTracking> tracking_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_RASTER_INVALIDATOR_H_
