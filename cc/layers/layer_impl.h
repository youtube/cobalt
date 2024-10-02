// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_IMPL_H_
#define CC_LAYERS_LAYER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "cc/base/region.h"
#include "cc/base/synced_property.h"
#include "cc/cc_export.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/input_handler.h"
#include "cc/layers/draw_mode.h"
#include "cc/layers/draw_properties.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/performance_properties.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/touch_action_region.h"
#include "cc/paint/element_id.h"
#include "cc/tiles/tile_priority.h"
#include "cc/trees/target_property.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {
class ClientResourceProvider;
class CompositorRenderPass;
}

namespace cc {

class AppendQuadsData;
struct LayerDebugInfo;
class LayerTreeImpl;
class MicroBenchmarkImpl;
class PrioritizedTile;
class SimpleEnclosedRegion;
class Tile;

enum ViewportLayerType {
  NOT_VIEWPORT_LAYER,
  INNER_VIEWPORT_CONTAINER,
  OUTER_VIEWPORT_CONTAINER,
  INNER_VIEWPORT_SCROLL,
  OUTER_VIEWPORT_SCROLL,
  LAST_VIEWPORT_LAYER_TYPE = OUTER_VIEWPORT_SCROLL,
};

class CC_EXPORT LayerImpl {
 public:
  static std::unique_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return base::WrapUnique(new LayerImpl(tree_impl, id));
  }

  LayerImpl(const LayerImpl&) = delete;
  virtual ~LayerImpl();

  LayerImpl& operator=(const LayerImpl&) = delete;

  int id() const { return layer_id_; }

  // Whether this layer is on the active tree, return false if it's on the
  // pending tree.
  bool IsActive() const;

  void SetHasTransformNode(bool val) { has_transform_node_ = val; }
  bool has_transform_node() const { return has_transform_node_; }

  void set_property_tree_sequence_number(int sequence_number) {}

  void SetTransformTreeIndex(int index);
  int transform_tree_index() const { return transform_tree_index_; }

  void SetClipTreeIndex(int index);
  int clip_tree_index() const { return clip_tree_index_; }

  void SetEffectTreeIndex(int index);
  int effect_tree_index() const { return effect_tree_index_; }
  int render_target_effect_tree_index() const;

  void SetScrollTreeIndex(int index);
  int scroll_tree_index() const { return scroll_tree_index_; }

  void SetOffsetToTransformParent(const gfx::Vector2dF& offset) {
    offset_to_transform_parent_ = offset;
  }
  gfx::Vector2dF offset_to_transform_parent() const {
    return offset_to_transform_parent_;
  }

  bool is_clipped() const { return draw_properties_.is_clipped; }

  LayerTreeImpl* layer_tree_impl() const { return layer_tree_impl_; }

  void PopulateSharedQuadState(viz::SharedQuadState* state,
                               bool contents_opaque) const;

  // If using these two, you need to override GetEnclosingRectInTargetSpace() to
  // use GetScaledEnclosingRectInTargetSpace(). To do otherwise may result in
  // inconsistent values, and drawing/clipping problems.
  void PopulateScaledSharedQuadState(viz::SharedQuadState* state,
                                     float layer_to_content_scale,
                                     bool contents_opaque) const;
  void PopulateScaledSharedQuadStateWithContentRects(
      viz::SharedQuadState* state,
      float layer_to_content_scale,
      const gfx::Rect& content_rect,
      const gfx::Rect& content_visible_rect,
      bool contents_opaque) const;

  // WillDraw must be called before AppendQuads. If WillDraw returns false,
  // AppendQuads and DidDraw will not be called. If WillDraw returns true,
  // DidDraw is guaranteed to be called before another WillDraw or before
  // the layer is destroyed.
  virtual bool WillDraw(DrawMode draw_mode,
                        viz::ClientResourceProvider* resource_provider);
  virtual void AppendQuads(viz::CompositorRenderPass* render_pass,
                           AppendQuadsData* append_quads_data) {}
  virtual void DidDraw(viz::ClientResourceProvider* resource_provider);

  // Verify that the resource ids in the quad are valid.
  void ValidateQuadResources(viz::DrawQuad* quad) const {
#if DCHECK_IS_ON()
    ValidateQuadResourcesInternal(quad);
#endif
  }

  virtual void GetContentsResourceId(viz::ResourceId* resource_id,
                                     gfx::Size* resource_size,
                                     gfx::SizeF* resource_uv_size) const;

  virtual void NotifyTileStateChanged(const Tile* tile) {}

  virtual bool IsScrollbarLayer() const;

  bool IsScrollerOrScrollbar() const;

  // Returns true if this layer has content to draw.
  void SetDrawsContent(bool draws_content);
  bool draws_content() const { return draws_content_; }

  // Make the layer hit testable.
  void SetHitTestable(bool should_hit_test);
  bool HitTestable() const;

  void SetBackgroundColor(SkColor4f background_color);
  SkColor4f background_color() const { return background_color_; }
  void SetSafeOpaqueBackgroundColor(SkColor4f background_color);
  SkColor4f safe_opaque_background_color() const {
    // Layer::SafeOpaqueBackgroundColor() should ensure this.
    DCHECK_EQ(contents_opaque(), safe_opaque_background_color_.isOpaque());
    return safe_opaque_background_color_;
  }

  // See Layer::SetContentsOpaque() and SetContentsOpaqueForText() for the
  // relationship between the two flags.
  void SetContentsOpaque(bool opaque);
  bool contents_opaque() const { return contents_opaque_; }
  void SetContentsOpaqueForText(bool opaque);
  bool contents_opaque_for_text() const { return contents_opaque_for_text_; }

  float Opacity() const;

  // Stable identifier for clients. See comment in cc/trees/element_id.h.
  void SetElementId(ElementId element_id);
  ElementId element_id() const { return element_id_; }

  bool IsAffectedByPageScale() const;

  bool Is3dSorted() const { return GetSortingContextId() != 0; }

  void SetShouldCheckBackfaceVisibility(bool should_check_backface_visibility) {
    should_check_backface_visibility_ = should_check_backface_visibility;
  }
  bool should_check_backface_visibility() const {
    return should_check_backface_visibility_;
  }

  bool ShowDebugBorders(DebugBorderType type) const;

  // The render surface which this layer draws into. This can be either owned by
  // the same layer or an ancestor of this layer.
  RenderSurfaceImpl* render_target();
  const RenderSurfaceImpl* render_target() const;

  DrawProperties& draw_properties() { return draw_properties_; }
  const DrawProperties& draw_properties() const { return draw_properties_; }

  gfx::Transform DrawTransform() const;
  gfx::Transform ScreenSpaceTransform() const;
  PerformanceProperties<LayerImpl>& performance_properties() {
    return performance_properties_;
  }

  // Setter for draw_properties_.
  void set_visible_layer_rect(const gfx::Rect& visible_rect) {
    draw_properties_.visible_layer_rect = visible_rect;
  }
  void set_clip_rect(const gfx::Rect& clip_rect) {
    draw_properties_.clip_rect = clip_rect;
  }

  // The following are shortcut accessors to get various information from
  // draw_properties_
  float draw_opacity() const { return draw_properties_.opacity; }
  bool screen_space_transform_is_animating() const {
    return draw_properties_.screen_space_transform_is_animating;
  }
  gfx::Rect clip_rect() const { return draw_properties_.clip_rect; }
  gfx::Rect visible_drawable_content_rect() const {
    return draw_properties_.visible_drawable_content_rect;
  }
  gfx::Rect visible_layer_rect() const {
    return draw_properties_.visible_layer_rect;
  }

  // The client should be responsible for setting bounds, content bounds and
  // contents scale to appropriate values. LayerImpl doesn't calculate any of
  // them from the other values.

  void SetBounds(const gfx::Size& bounds);
  gfx::Size bounds() const;

  void set_is_inner_viewport_scroll_layer() {
    is_inner_viewport_scroll_layer_ = true;
  }

  void SetCurrentScrollOffset(const gfx::PointF& scroll_offset);

  // Returns the delta of the scroll that was outside of the bounds of the
  // initial scroll
  gfx::Vector2dF ScrollBy(const gfx::Vector2dF& scroll);

  // Called during a commit or activation, after the property trees are pushed.
  // It detects changes of scrollable status and scroll container size in the
  // scroll property, and invalidate scrollbar geometries etc. for the changes.
  void UpdateScrollable();

  // Some properties on the LayerImpl are rarely set, and so are bundled
  // under a single unique_ptr.
  struct RareProperties {
    // The bounds of elements marked for potential region capture, stored in
    // the coordinate space of this layer.
    viz::RegionCaptureBounds capture_bounds;
    Region non_fast_scrollable_region;
    Region wheel_event_handler_region;
  };

  RareProperties& EnsureRareProperties() {
    if (!rare_properties_)
      rare_properties_ = std::make_unique<RareProperties>();

    return *rare_properties_;
  }

  void ResetRareProperties() { rare_properties_.reset(); }

  void SetNonFastScrollableRegion(const Region& region) {
    if (rare_properties_ || !region.IsEmpty())
      EnsureRareProperties().non_fast_scrollable_region = region;
  }
  const Region& non_fast_scrollable_region() const {
    return rare_properties_ ? rare_properties_->non_fast_scrollable_region
                            : Region::Empty();
  }

  void SetTouchActionRegion(TouchActionRegion);
  const TouchActionRegion& touch_action_region() const {
    return touch_action_region_;
  }
  const Region& GetAllTouchActionRegions() const;
  bool has_touch_action_regions() const {
    return !touch_action_region_.IsEmpty();
  }

  void SetCaptureBounds(viz::RegionCaptureBounds bounds);
  const viz::RegionCaptureBounds* capture_bounds() const {
    return rare_properties_ ? &rare_properties_->capture_bounds : nullptr;
  }

  // Set or get the region that contains wheel event handler.
  // The |wheel_event_handler_region| specify the area where wheel event handler
  // could block impl scrolling.
  void SetWheelEventHandlerRegion(const Region& wheel_event_handler_region) {
    if (rare_properties_ || !wheel_event_handler_region.IsEmpty()) {
      EnsureRareProperties().wheel_event_handler_region =
          wheel_event_handler_region;
    }
  }
  const Region& wheel_event_handler_region() const {
    return rare_properties_ ? rare_properties_->wheel_event_handler_region
                            : Region::Empty();
  }

  // The main thread may commit multiple times before the impl thread actually
  // draws, so we need to accumulate (i.e. union) any update changes that have
  // occurred on the main thread until we draw.
  // Note this rect is in layer space (not content space).
  void UnionUpdateRect(const gfx::Rect& update_rect);
  const gfx::Rect& update_rect() const { return update_rect_; }

  // Denotes an area that is damaged and needs redraw. This is in the layer's
  // space. By default returns empty rect, but can be overridden by subclasses
  // as appropriate.
  virtual gfx::Rect GetDamageRect() const;

  // This includes |layer_property_changed_not_from_property_trees_| and
  // property_trees changes.
  bool LayerPropertyChanged() const;
  bool LayerPropertyChangedFromPropertyTrees() const;
  // Only checks |layer_property_changed_not_from_property_trees_|. Used in
  // damage_tracker to determine if there is a contributing content damage not
  // from property_trees changes in animaiton.
  bool LayerPropertyChangedNotFromPropertyTrees() const;

  virtual void ResetChangeTracking();

  virtual SimpleEnclosedRegion VisibleOpaqueRegion() const;

  virtual void DidBecomeActive() {}

  virtual void DidBeginTracing();

  // Release resources held by this layer. Called when the output surface
  // that rendered this layer was lost.
  virtual void ReleaseResources();

  // Releases resources in response to memory pressure. The default
  // implementation just calls ReleaseResources() and subclasses will override
  // if that's not appropriate.
  virtual void OnPurgeMemory();

  // Release tile resources held by this layer. Called when a rendering mode
  // switch has occurred and tiles are no longer valid.
  virtual void ReleaseTileResources();

  // Recreate tile resources held by this layer after they were released by a
  // ReleaseTileResources call.
  virtual void RecreateTileResources();

  virtual std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const;
  virtual void PushPropertiesTo(LayerImpl* layer);

  // Internal to property tree construction (which only happens in tests on a
  // LayerImpl tree. See Layer::IsSnappedToPixelGridInTarget() for explanation,
  // as this mirrors that method.
  virtual bool IsSnappedToPixelGridInTarget();

  virtual void GetAllPrioritizedTilesForTracing(
      std::vector<PrioritizedTile>* prioritized_tiles) const;
  virtual void AsValueInto(base::trace_event::TracedValue* dict) const;
  std::string ToString() const;

  virtual size_t GPUMemoryUsageInBytes() const;

  // Mark a layer on pending tree that needs to push its properties to the
  // active tree. These properties should not be changed during pending tree
  // lifetime, and only changed by being pushed from the main thread. There are
  // three cases where this function needs to be called: when main thread layer
  // has properties that need to be pushed, when a new LayerImpl is created
  // on pending tree when syncing layers from main thread, or when we recompute
  // visible layer properties on the pending tree.
  void SetNeedsPushProperties();

  virtual void RunMicroBenchmark(MicroBenchmarkImpl* benchmark);

  void UpdateDebugInfo(LayerDebugInfo* debug_info);

  void set_contributes_to_drawn_render_surface(bool is_member) {
    contributes_to_drawn_render_surface_ = is_member;
  }

  bool contributes_to_drawn_render_surface() const {
    return contributes_to_drawn_render_surface_;
  }

  void set_may_contain_video(bool yes) { may_contain_video_ = yes; }
  bool may_contain_video() const { return may_contain_video_; }

  // Layers that share a sorting context id will be sorted together in 3d
  // space.  0 is a special value that means this layer will not be sorted and
  // will be drawn in paint order.
  int GetSortingContextId() const;

  // Get the correct invalidation region instead of conservative Rect
  // for layers that provide it.
  virtual Region GetInvalidationRegionForDebugging();

  // Returns the visible rect that is used by damage tracker. This damage rect
  // is computed as an eclosing rect of actual content size x transform to
  // target space, which can be different from visible_drawing_content_rect. For
  // example, the actual tile size of picture layer in the target surface can be
  // bigger when e.g. the layer's raster scale is different from the scale of
  // the layer's draw transform.
  // If you override this, and are making use of
  // PopulateScaledSharedQuadState(), make sure you call
  // GetScaledEnclosingVisibleRectInTargetSpace(). See comment for
  // PopulateScaledSharedQuadState().
  virtual gfx::Rect GetEnclosingVisibleRectInTargetSpace() const;

  // Returns the visible bounds of this layer in target space when scaled by
  // |scale|.  This function scales in the same way as
  // PopulateScaledSharedQuadStateQuadState(). See
  // PopulateScaledSharedQuadStateQuadState() for more details.
  gfx::Rect GetScaledEnclosingVisibleRectInTargetSpace(float scale) const;

  // GetIdealContentsScale() returns the ideal 2D scale, clamped to not exceed
  // GetPreferredRasterScale().
  // GetIdealContentsScaleKey() returns the maximum component, a fallback to
  // uniform scale for callers that don't support 2d scales yet.
  // TODO(crbug.com/1196414): Remove GetIdealContentsScaleKey() in favor of
  // GetIdealContentsScale().
  gfx::Vector2dF GetIdealContentsScale() const;
  float GetIdealContentsScaleKey() const;

  void NoteLayerPropertyChanged();
  void NoteLayerPropertyChangedFromPropertyTrees();

  ElementListType GetElementTypeForAnimation() const;

  void set_needs_show_scrollbars(bool yes) { needs_show_scrollbars_ = yes; }
  bool needs_show_scrollbars() { return needs_show_scrollbars_; }

  void set_raster_even_if_not_drawn(bool yes) {
    raster_even_if_not_drawn_ = yes;
  }
  bool raster_even_if_not_drawn() const { return raster_even_if_not_drawn_; }

  void EnsureValidPropertyTreeIndices() const;

  // TODO(sunxd): Remove this function and replace it with visitor pattern.
  virtual bool is_surface_layer() const;

  int CalculateJitter();

  std::string DebugName() const;

  virtual gfx::ContentColorUsage GetContentColorUsage() const;

  virtual void NotifyKnownResourceIdsBeforeAppendQuads(
      const base::flat_set<viz::ViewTransitionElementResourceId>&
          known_resource_ids) {}

  virtual viz::ViewTransitionElementResourceId ViewTransitionResourceId() const;

 protected:
  // When |will_always_push_properties| is true, the layer will not itself set
  // its SetNeedsPushProperties() state, as it expects to be always pushed to
  // the active tree regardless.
  LayerImpl(LayerTreeImpl* layer_impl,
            int id,
            bool will_always_push_properties = false);

  // Get the color and size of the layer's debug border.
  virtual void GetDebugBorderProperties(SkColor4f* color, float* width) const;

  void AppendDebugBorderQuad(viz::CompositorRenderPass* render_pass,
                             const gfx::Rect& quad_rect,
                             const viz::SharedQuadState* shared_quad_state,
                             AppendQuadsData* append_quads_data) const;
  void AppendDebugBorderQuad(viz::CompositorRenderPass* render_pass,
                             const gfx::Rect& quad_rect,
                             const viz::SharedQuadState* shared_quad_state,
                             AppendQuadsData* append_quads_data,
                             SkColor4f color,
                             float width) const;

  static float GetPreferredRasterScale(
      gfx::Vector2dF raster_space_scale_factor);

 private:
  void ValidateQuadResourcesInternal(viz::DrawQuad* quad) const;
  gfx::Transform GetScaledDrawTransform(float layer_to_content_scale) const;

  virtual const char* LayerTypeAsString() const;

  const int layer_id_;
  const raw_ptr<LayerTreeImpl> layer_tree_impl_;
  const bool will_always_push_properties_ : 1;

  // Properties synchronized from the associated Layer.
  gfx::Size bounds_;

  gfx::Vector2dF offset_to_transform_parent_;

  // These fields are copies of |container_bounds|, |bounds| and |scrollable|
  // fields in the associated ScrollNode, and are updated in UpdateScrollable().
  // The copy is for change detection only.
  // TODO(wangxianzhu): Actually we only need scroll_container_bounds_ in
  // pre-CompositeAfterPaint where the scroll node is associated with the
  // scrolling contents layer, and only need scroll_contents_bounds_ in
  // CompositeAfterPaint where the scroll node is associated with the scroll
  // container layer. Remove scroll_container_bounds_ when we launch CAP.
  gfx::Size scroll_container_bounds_;
  gfx::Size scroll_contents_bounds_;
  bool scrollable_ : 1;

  // Tracks if drawing-related properties have changed since last redraw.
  // TODO(wutao): We want to distinquish the sources of change so that we can
  // reuse the cache of render pass. For example, we can reuse the cache when
  // transform and opacity changing on a surface during animation. Currently
  // |layer_property_changed_from_property_trees_| does not mean the layer is
  // damaged from animation. We need better mechanism to explicitly capture
  // damage from animations. http://crbug.com/755828.
  bool layer_property_changed_not_from_property_trees_ : 1;
  bool layer_property_changed_from_property_trees_ : 1;

  bool may_contain_video_ : 1;
  bool contents_opaque_ : 1;
  bool contents_opaque_for_text_ : 1;
  bool should_check_backface_visibility_ : 1;
  bool draws_content_ : 1;
  bool contributes_to_drawn_render_surface_ : 1;

  // Tracks if this layer should participate in hit testing.
  bool hit_testable_ : 1;

  bool is_inner_viewport_scroll_layer_ : 1;

  TouchActionRegion touch_action_region_;

  SkColor4f background_color_;
  SkColor4f safe_opaque_background_color_;

  int transform_tree_index_;
  int effect_tree_index_;
  int clip_tree_index_;
  int scroll_tree_index_;

  std::unique_ptr<RareProperties> rare_properties_;

 protected:
  friend class TreeSynchronizer;

  DrawMode current_draw_mode_;
  EffectTree& GetEffectTree() const;
  PropertyTrees* GetPropertyTrees() const;
  ClipTree& GetClipTree() const;
  ScrollTree& GetScrollTree() const;
  TransformTree& GetTransformTree() const;

 private:
  ElementId element_id_;
  // Rect indicating what was repainted/updated during update.
  // Note that plugin layers bypass this and leave it empty.
  // This is in the layer's space.
  gfx::Rect update_rect_;

  // Group of properties that need to be computed based on the layer tree
  // hierarchy before layers can be drawn.
  DrawProperties draw_properties_;
  PerformanceProperties<LayerImpl> performance_properties_;

  std::unique_ptr<LayerDebugInfo> debug_info_;

  // Cache of all regions represented by any touch action from
  // |touch_action_region_|.
  mutable std::unique_ptr<Region> all_touch_action_regions_;

  bool needs_push_properties_ : 1;

  // The needs_show_scrollbars_ bit tracks a pending request to show the overlay
  // scrollbars. It's set by UpdateScrollable() on the scroll layer (not the
  // scrollbar layers) and consumed by LayerTreeImpl::PushPropertiesTo() and
  // LayerTreeImpl::HandleScrollbarShowRequests().
  bool needs_show_scrollbars_ : 1;

  // This is set for layers that have a property because of which they are not
  // drawn (singular transforms), but they can become visible soon (the property
  // is being animated). For this reason, while these layers are not drawn, they
  // are still rasterized.
  bool raster_even_if_not_drawn_ : 1;

  bool has_transform_node_ : 1;
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_IMPL_H_
