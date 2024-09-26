// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_LAYER_IMPL_BASE_H_
#define CC_LAYERS_SCROLLBAR_LAYER_IMPL_BASE_H_

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "cc/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

class LayerTreeImpl;

class CC_EXPORT ScrollbarLayerImplBase : public LayerImpl {
 public:
  ScrollbarLayerImplBase(const ScrollbarLayerImplBase&) = delete;
  ScrollbarLayerImplBase& operator=(const ScrollbarLayerImplBase&) = delete;

  ElementId scroll_element_id() const { return scroll_element_id_; }
  void SetScrollElementId(ElementId scroll_element_id);

  // The following setters should be called when updating scrollbar geometries
  // (see: LayerTreeImpl::UpdateScrollbarGeometries).
  bool SetCurrentPos(float current_pos);
  void SetClipLayerLength(float clip_layer_length);
  void SetScrollLayerLength(float scroll_layer_length);
  void SetVerticalAdjust(float vertical_adjust);

  float current_pos() const;
  float clip_layer_length() const;
  float scroll_layer_length() const;
  float vertical_adjust() const;

  bool is_overlay_scrollbar() const { return is_overlay_scrollbar_; }
  void set_is_overlay_scrollbar(bool is_overlay) {
    is_overlay_scrollbar_ = is_overlay;
  }

  ScrollbarOrientation orientation() const { return orientation_; }
  bool is_left_side_vertical_scrollbar() const {
    return is_left_side_vertical_scrollbar_;
  }

  bool CanScrollOrientation() const;

  void PushPropertiesTo(LayerImpl* layer) override;

  // Thumb quad rect in layer space.
  virtual gfx::Rect ComputeThumbQuadRect() const;
  virtual gfx::Rect ComputeHitTestableThumbQuadRect() const;
  gfx::Rect ComputeExpandedThumbQuadRect() const;

  float thumb_thickness_scale_factor() {
    return thumb_thickness_scale_factor_;
  }
  void SetThumbThicknessScaleFactor(float thumb_thickness_scale_factor);

  virtual int ThumbThickness() const = 0;

  void SetOverlayScrollbarLayerOpacityAnimated(float opacity);

  virtual LayerTreeSettings::ScrollbarAnimator GetScrollbarAnimator() const;

  virtual float TrackLength() const = 0;
  virtual int ThumbLength() const = 0;
  virtual gfx::Rect BackButtonRect() const;
  virtual gfx::Rect ForwardButtonRect() const;
  virtual gfx::Rect BackTrackRect() const;
  virtual gfx::Rect ForwardTrackRect() const;
  virtual bool SupportsDragSnapBack() const;
  virtual bool JumpOnTrackClick() const;
  virtual ScrollbarPart IdentifyScrollbarPart(
      const gfx::PointF position_in_widget) const;
  // Only PaintedOverlayScrollbar(Aura Overlay Scrollbar) need to know
  // tickmarks's state.
  virtual bool HasFindInPageTickmarks() const;

  // Mac overlay scrollbars are faded during paint but the compositor layer is
  // always fully opaque where as Aura scrollbars fade by animating the layer
  // opacity. This method will return the user visible opacity of an overlay
  // scrollbar regardless of the underlying mechanism or platform.
  virtual float OverlayScrollbarOpacity() const;

  bool IsFluentScrollbarEnabled() const;

 protected:
  ScrollbarLayerImplBase(LayerTreeImpl* tree_impl,
                         int id,
                         ScrollbarOrientation orientation,
                         bool is_left_side_vertical_scrollbar,
                         bool is_overlay);
  ~ScrollbarLayerImplBase() override;

  virtual int TrackStart() const = 0;
  // Indicates whether the thumb length can be changed without going back to the
  // main thread.
  virtual bool IsThumbResizable() const = 0;

 private:
  bool IsScrollbarLayer() const final;

  gfx::Rect ComputeThumbQuadRectWithThumbThicknessScale(
      float thumb_thickness_scale_factor) const;

  ElementId scroll_element_id_;
  bool is_overlay_scrollbar_;

  float thumb_thickness_scale_factor_;
  float current_pos_;
  float clip_layer_length_;
  float scroll_layer_length_;
  ScrollbarOrientation orientation_;
  bool is_left_side_vertical_scrollbar_;

  // Difference between the clip layer's height and the visible viewport
  // height (which may differ in the presence of top-controls hiding).
  float vertical_adjust_;

  FRIEND_TEST_ALL_PREFIXES(ScrollbarLayerTest,
                           ScrollElementIdPushedAcrossCommit);
};

inline ScrollbarLayerImplBase* ToScrollbarLayer(LayerImpl* layer) {
  DCHECK(layer->IsScrollbarLayer());
  return static_cast<ScrollbarLayerImplBase*>(layer);
}

inline const ScrollbarLayerImplBase* ToScrollbarLayer(const LayerImpl* layer) {
  DCHECK(layer->IsScrollbarLayer());
  return static_cast<const ScrollbarLayerImplBase*>(layer);
}

using ScrollbarSet = base::flat_set<ScrollbarLayerImplBase*>;

}  // namespace cc

#endif  // CC_LAYERS_SCROLLBAR_LAYER_IMPL_BASE_H_
