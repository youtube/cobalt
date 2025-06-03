// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PENDING_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PENDING_LAYER_H_

#include "base/check_op.h"
#include "cc/input/layer_selection_bound.h"
#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"
#include "third_party/blink/renderer/platform/graphics/lcd_text_preference.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/ref_counted_property_tree_state.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
class LayerTreeHost;
}

namespace blink {

// A pending layer is a collection of paint chunks that will end up in the same
// cc::Layer.
class PLATFORM_EXPORT PendingLayer {
 public:
  enum CompositingType {
    kScrollHitTestLayer,
    kForeignLayer,
    kScrollbarLayer,
    kOverlap,
    kOther,
  };

  PendingLayer(scoped_refptr<const PaintArtifact>,
               const PaintChunk& first_chunk);

  // Returns the offset/bounds for the final cc::Layer, rounded if needed.
  gfx::Vector2dF LayerOffset() const;
  gfx::Size LayerBounds() const;

  const gfx::RectF& BoundsForTesting() const { return bounds_; }

  const gfx::RectF& RectKnownToBeOpaque() const {
    return rect_known_to_be_opaque_;
  }
  bool TextKnownToBeOnOpaqueBackground() const {
    return text_known_to_be_on_opaque_background_;
  }
  const PaintChunkSubset& Chunks() const { return chunks_; }
  const PropertyTreeState GetPropertyTreeState() const {
    return property_tree_state_.GetPropertyTreeState();
  }
  const gfx::Vector2dF& OffsetOfDecompositedTransforms() const {
    return offset_of_decomposited_transforms_;
  }
  PaintPropertyChangeType ChangeOfDecompositedTransforms() const {
    return change_of_decomposited_transforms_;
  }
  CompositingType GetCompositingType() const { return compositing_type_; }
  cc::HitTestOpaqueness GetHitTestOpaqueness() const {
    return hit_test_opaqueness_;
  }

  void SetCompositingType(CompositingType new_type) {
    compositing_type_ = new_type;
  }

  void SetPaintArtifact(scoped_refptr<const PaintArtifact> paint_artifact) {
    chunks_.SetPaintArtifact(std::move(paint_artifact));
  }

  using IsCompositedScrollFunction =
      PropertyTreeState::IsCompositedScrollFunction;

  // Merges |guest| into |this| if it can, by appending chunks of |guest|
  // after chunks of |this|, with appropriate space conversion applied to
  // both layers from their original property tree states to |merged_state|.
  // Returns whether the merge is successful.
  bool Merge(const PendingLayer& guest,
             LCDTextPreference lcd_text_preference,
             IsCompositedScrollFunction);

  // Returns true if `guest` that could be upcasted with decomposited blend
  // mode can be merged into `this`.
  bool CanMergeWithDecompositedBlendMode(const PendingLayer& guest,
                                         const PropertyTreeState& upcast_state,
                                         IsCompositedScrollFunction) const;

  // Mutate this layer's property tree state to a more general (shallower)
  // state, thus the name "upcast". The concrete effect of this is to
  // "decomposite" some of the properties, so that fewer properties will be
  // applied by the compositor, and more properties will be applied internally
  // to the chunks as Skia commands.
  void Upcast(const PropertyTreeState&);

  const PaintChunk& FirstPaintChunk() const;
  const DisplayItem& FirstDisplayItem() const;

  bool Matches(const PendingLayer& old_pending_layer) const;

  const TransformPaintPropertyNode& ScrollTranslationForScrollHitTestLayer()
      const;

  std::unique_ptr<JSONObject> ToJSON() const;
  String DebugName() const;
  DOMNodeId OwnerNodeId() const;

  void ForceDrawsContent() { draws_content_ = true; }
  bool DrawsContent() const { return draws_content_; }

  bool ChunkRequiresOwnLayer() const {
    bool result = compositing_type_ != kOverlap && compositing_type_ != kOther;
#if DCHECK_IS_ON()
    if (result) {
      DCHECK(!content_layer_client_);
      DCHECK_EQ(chunks_.size(), 1u);
    } else {
      DCHECK(!cc_layer_ || UsesSolidColorLayer());
      DCHECK_GE(chunks_.size(), 1u);
    }
#endif
    return result;
  }

  bool MightOverlap(const PendingLayer& other) const;

  static void DecompositeTransforms(Vector<PendingLayer>& pending_layers);

  // This is valid only when SetCclayer() or SetContentLayerClient() has been
  // called.
  cc::Layer& CcLayer() const {
    if (content_layer_client_)
      return content_layer_client_->Layer();
    DCHECK(cc_layer_);
    return *cc_layer_;
  }

  ContentLayerClientImpl* GetContentLayerClient() const {
    return content_layer_client_.get();
  }

  void UpdateCcLayerHitTestOpaqueness() const;

  // For this PendingLayer, creates a composited layer or uses the existing
  // one in |old_pending_layer|, and updates the layer according to the current
  // contents and properties of this PendingLayer.
  void UpdateCompositedLayer(PendingLayer* old_pending_layer,
                             cc::LayerSelection&,
                             bool tracks_raster_invalidations,
                             cc::LayerTreeHost*);

  // A lighter version of UpdateCompositedLayer(). Called when the existing
  // composited layer has only repainted since the last update.
  void UpdateCompositedLayerForRepaint(
      scoped_refptr<const PaintArtifact> repainted_artifact,
      cc::LayerSelection&);

  SkColor4f ComputeBackgroundColor() const;

  // True if a solid color chunk exists that makes this entire layer
  // draw a solid color (see comment above `solid_color_chunk_index_`).
  bool IsSolidColor() const { return solid_color_chunk_index_ != kNotFound; }

 private:
  // Checks basic merge-ability with `guest` and calls
  // PropertyTreeState::CanUpcastWith().
  absl::optional<PropertyTreeState> CanUpcastWith(
      const PendingLayer& guest,
      const PropertyTreeState& guest_state,
      IsCompositedScrollFunction is_comosited_scroll) const;

  bool CanMerge(const PendingLayer& guest,
                LCDTextPreference lcd_text_preference,
                IsCompositedScrollFunction,
                gfx::RectF& merged_bounds,
                PropertyTreeState& merged_state,
                gfx::RectF& merged_rect_known_to_be_opaque,
                bool& merged_text_known_to_be_on_opaque_background,
                wtf_size_t& merged_solid_color_chunk_index,
                cc::HitTestOpaqueness& merged_hit_test_opaqueness) const;

  gfx::RectF MapRectKnownToBeOpaque(
      const PropertyTreeState& new_state,
      const FloatClipRect& mapped_layer_bounds) const;

  bool PropertyTreeStateChanged(const PendingLayer* old_pending_layer) const;

  // The following methods are called by UpdateCompositedLayer(), each for a
  // particular type of composited layer.
  void UpdateForeignLayer();
  void UpdateScrollHitTestLayer(PendingLayer* old_pending_layer);
  void UpdateScrollbarLayer(PendingLayer* old_pending_layer);
  void UpdateContentLayer(PendingLayer* old_pending_layer,
                          bool tracks_raster_invalidations);
  void UpdateSolidColorLayer(PendingLayer* old_pending_layer);

  void UpdateLayerProperties(cc::LayerSelection&, bool selection_only);

  bool UsesSolidColorLayer() const;
  SkColor4f GetSolidColor() const;

  // The rects are in the space of property_tree_state.
  gfx::RectF bounds_;
  gfx::RectF rect_known_to_be_opaque_;
  bool has_text_ = false;
  bool draws_content_ = false;
  bool text_known_to_be_on_opaque_background_ = false;
  bool has_decomposited_blend_mode_ = false;
  // If not kNotFound, this is the index of the chunk that makes this layer
  // solid color. The solid color chunk must be the last drawable chunk and
  // must draw a solid color that fully covers this pending layer.
  wtf_size_t solid_color_chunk_index_ = kNotFound;
  PaintChunkSubset chunks_;
  RefCountedPropertyTreeState property_tree_state_;
  gfx::Vector2dF offset_of_decomposited_transforms_;
  PaintPropertyChangeType change_of_decomposited_transforms_ =
      PaintPropertyChangeType::kUnchanged;
  CompositingType compositing_type_ = kOther;
  cc::HitTestOpaqueness hit_test_opaqueness_ =
      cc::HitTestOpaqueness::kTransparent;

  // This is set to non-null after layerization if ChunkRequiresOwnLayer() or
  // UsesSolidColorLayer() is true.
  scoped_refptr<cc::Layer> cc_layer_;
  // This is set to non-null after layerization if !ChunkRequiresOwnLayer() and
  // UsesSolidColorLayer() is false.
  std::unique_ptr<ContentLayerClientImpl> content_layer_client_;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const PendingLayer&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PENDING_LAYER_H_
