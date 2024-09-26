// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_

#include <algorithm>

#include "components/viz/common/view_transition_element_resource_id.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/view_transition_element_id.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"

namespace blink {

class ClipPaintPropertyNodeOrAlias;
class PropertyTreeState;

// Effect nodes are abstraction of isolated groups, along with optional effects
// that can be applied to the composited output of the group.
//
// The effect tree is rooted at a node with no parent. This root node should
// not be modified.
class EffectPaintPropertyNode;

class PLATFORM_EXPORT EffectPaintPropertyNodeOrAlias
    : public PaintPropertyNode<EffectPaintPropertyNodeOrAlias,
                               EffectPaintPropertyNode> {
 public:
  // Checks if the accumulated effect from |this| to |relative_to_state
  // .Effect()| has changed, at least significance of |change|, in the space of
  // |relative_to_state.Transform()|. We check for changes of not only effect
  // nodes, but also LocalTransformSpace relative to |relative_to_state
  // .Transform()| of the effect nodes having filters that move pixels. Change
  // of OutputClip is not checked and the caller should check in other ways.
  // |transform_not_to_check| specifies the transform node that the caller has
  // checked or will check its change in other ways and this function should
  // treat it as unchanged.
  bool Changed(
      PaintPropertyChangeType change,
      const PropertyTreeState& relative_to_state,
      const TransformPaintPropertyNodeOrAlias* transform_not_to_check) const;

  void ClearChangedToRoot(int sequence_number) const;

 protected:
  using PaintPropertyNode::PaintPropertyNode;
};

class EffectPaintPropertyNodeAlias : public EffectPaintPropertyNodeOrAlias {
 public:
  static scoped_refptr<EffectPaintPropertyNodeAlias> Create(
      const EffectPaintPropertyNodeOrAlias& parent) {
    return base::AdoptRef(new EffectPaintPropertyNodeAlias(parent));
  }

 private:
  explicit EffectPaintPropertyNodeAlias(
      const EffectPaintPropertyNodeOrAlias& parent)
      : EffectPaintPropertyNodeOrAlias(parent, kParentAlias) {}
};

class PLATFORM_EXPORT EffectPaintPropertyNode
    : public EffectPaintPropertyNodeOrAlias {
 public:
  struct AnimationState {
    AnimationState() {}
    bool is_running_opacity_animation_on_compositor = false;
    bool is_running_filter_animation_on_compositor = false;
    bool is_running_backdrop_filter_animation_on_compositor = false;
    STACK_ALLOCATED();
  };

  struct BackdropFilterInfo {
    CompositorFilterOperations operations;
    gfx::RRectF bounds;
    // The compositor element id for any masks that are applied to elements that
    // also have backdrop-filters applied.
    CompositorElementId mask_element_id;

    USING_FAST_MALLOC(BackdropFilterInfo);
  };

  // To make it less verbose and more readable to construct and update a node,
  // a struct with default values is used to represent the state.
  struct PLATFORM_EXPORT State {
    DISALLOW_NEW();

   public:
    // The local transform space serves two purposes:
    // 1. Assign a depth mapping for 3D depth sorting against other paint chunks
    //    and effects under the same parent.
    // 2. Some effects are spatial (namely blur filter and reflection), the
    //    effect parameters will be specified in the local space.
    scoped_refptr<const TransformPaintPropertyNodeOrAlias>
        local_transform_space;
    // The output of the effect can be optionally clipped when composited onto
    // the current backdrop.
    scoped_refptr<const ClipPaintPropertyNodeOrAlias> output_clip;
    // Optionally a number of effects can be applied to the composited output.
    // The chain of effects will be applied in the following order:
    // === Begin of effects ===
    CompositorFilterOperations filter;
    std::unique_ptr<BackdropFilterInfo> backdrop_filter_info;
    float opacity = 1;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;
    // === End of effects ===
    CompositingReasons direct_compositing_reasons = CompositingReason::kNone;
    CompositorElementId compositor_element_id;

    // An identifier for a view transition element. `id.valid()` returns true if
    // this has been set, and false otherwise.
    blink::ViewTransitionElementId view_transition_element_id;

    // An identifier to tag transition element resources generated and cached in
    // the Viz process. This generated resource can be used as content for other
    // elements.
    viz::ViewTransitionElementResourceId view_transition_element_resource_id;

    PaintPropertyChangeType ComputeChange(
        const State& other,
        const AnimationState& animation_state) const;

    PaintPropertyChangeType ComputeOpacityChange(
        float opacity,
        const AnimationState& animation_state) const;

    // Opacity change is simple if
    // - opacity doesn't change from or to 1, or
    // - there was and is active opacity animation, or
    // TODO(crbug.com/1285498): Optimize for will-change: opacity.
    // The rule is because whether opacity is 1 affects whether the effect
    // should create a render surface if there is no active opacity animation.
    static bool IsOpacityChangeSimple(
        float opacity,
        float new_opacity,
        CompositingReasons direct_compositing_reasons,
        CompositingReasons new_direct_compositing_reasons);
  };

  // This node is really a sentinel, and does not represent a real effect.
  static const EffectPaintPropertyNode& Root();

  static scoped_refptr<EffectPaintPropertyNode> Create(
      const EffectPaintPropertyNodeOrAlias& parent,
      State&& state) {
    return base::AdoptRef(
        new EffectPaintPropertyNode(&parent, std::move(state)));
  }

  PaintPropertyChangeType Update(
      const EffectPaintPropertyNodeOrAlias& parent,
      State&& state,
      const AnimationState& animation_state = AnimationState()) {
    auto parent_changed = SetParent(parent);
    auto state_changed = state_.ComputeChange(state, animation_state);
    if (state_changed != PaintPropertyChangeType::kUnchanged) {
      state_ = std::move(state);
      AddChanged(state_changed);
    }
    return std::max(parent_changed, state_changed);
  }

  PaintPropertyChangeType DirectlyUpdateOpacity(
      float opacity,
      const AnimationState& animation_state);

  const EffectPaintPropertyNode& Unalias() const = delete;
  bool IsParentAlias() const = delete;

  const TransformPaintPropertyNodeOrAlias& LocalTransformSpace() const {
    return *state_.local_transform_space;
  }
  const ClipPaintPropertyNodeOrAlias* OutputClip() const {
    return state_.output_clip.get();
  }

  SkBlendMode BlendMode() const {
    return state_.blend_mode;
  }
  float Opacity() const {
    return state_.opacity;
  }
  const CompositorFilterOperations& Filter() const {
    return state_.filter;
  }

  const CompositorFilterOperations* BackdropFilter() const {
    if (!state_.backdrop_filter_info)
      return nullptr;
    DCHECK(!state_.backdrop_filter_info->operations.IsEmpty());
    return &state_.backdrop_filter_info->operations;
  }

  const gfx::RRectF& BackdropFilterBounds() const {
    DCHECK(state_.backdrop_filter_info);
    return state_.backdrop_filter_info->bounds;
  }

  CompositorElementId BackdropMaskElementId() const {
    DCHECK(state_.backdrop_filter_info);
    return state_.backdrop_filter_info->mask_element_id;
  }

  bool HasFilterThatMovesPixels() const {
    return state_.filter.HasFilterThatMovesPixels();
  }

  bool HasRealEffects() const {
    return Opacity() != 1.0f || BlendMode() != SkBlendMode::kSrcOver ||
           !Filter().IsEmpty() || BackdropFilter();
  }

  bool IsOpacityOnly() const {
    return BlendMode() == SkBlendMode::kSrcOver && Filter().IsEmpty() &&
           !BackdropFilter();
  }

  // Returns a rect covering the pixels that can be affected by pixels in
  // |inputRect|. The rects are in the space of localTransformSpace.
  gfx::RectF MapRect(const gfx::RectF& input_rect) const;

  bool HasDirectCompositingReasons() const {
    return state_.direct_compositing_reasons != CompositingReason::kNone;
  }
  bool RequiresCompositingForBackdropFilterMask() const {
    return state_.direct_compositing_reasons &
           CompositingReason::kBackdropFilterMask;
  }

  bool FlattensAtLeafOf3DScene() const {
    return state_.direct_compositing_reasons &
           CompositingReason::kTransform3DSceneLeaf;
  }

  bool HasActiveOpacityAnimation() const {
    return state_.direct_compositing_reasons &
           CompositingReason::kActiveOpacityAnimation;
  }
  bool HasActiveFilterAnimation() const {
    return state_.direct_compositing_reasons &
           CompositingReason::kActiveFilterAnimation;
  }
  bool HasActiveBackdropFilterAnimation() const {
    return state_.direct_compositing_reasons &
           CompositingReason::kActiveBackdropFilterAnimation;
  }

  bool RequiresCompositingForWillChangeOpacity() const {
    return state_.direct_compositing_reasons &
           CompositingReason::kWillChangeOpacity;
  }
  bool RequiresCompositingForWillChangeFilter() const {
    return state_.direct_compositing_reasons &
           CompositingReason::kWillChangeFilter;
  }
  bool RequiresCompositingForWillChangeBackdropFilter() const {
    return state_.direct_compositing_reasons &
           CompositingReason::kWillChangeBackdropFilter;
  }

  // True if opacity is not 1.0, or could become non-1.0 without a compositing
  // update via a compositor animation or direct update.
  bool MayHaveOpacity() const {
    return Opacity() != 1.0f || HasActiveOpacityAnimation() ||
           RequiresCompositingForWillChangeOpacity();
  }
  // True if the filter is not empty, or could become non-empty without a
  // compositing update via a compositor animation or direct update.
  bool MayHaveFilter() const {
    return !Filter().IsEmpty() || HasActiveFilterAnimation() ||
           RequiresCompositingForWillChangeFilter();
  }
  // True if the backdrop filter is not empty, or could become non-empty
  // without a compositing update via a compositor animation or direct update.
  bool MayHaveBackdropFilter() const {
    return BackdropFilter() || HasActiveBackdropFilterAnimation() ||
           RequiresCompositingForWillChangeBackdropFilter();
  }

  // Whether the effect node uses the backdrop as an input. This includes
  // exotic blending modes and backdrop filters.
  bool MayHaveBackdropEffect() const {
    return BlendMode() != SkBlendMode::kSrcOver || MayHaveBackdropFilter();
  }

  // True if this effect can produce drawable content on its own. For example,
  // a drop-shadow filter will draw a drop shadow even if the filtered content
  // is entirely empty.
  bool DrawsContent() const {
    return MayHaveFilter() || MayHaveBackdropEffect() ||
           ViewTransitionElementId().valid();
  }

  CompositingReasons DirectCompositingReasonsForDebugging() const {
    return state_.direct_compositing_reasons;
  }

  const CompositorElementId& GetCompositorElementId() const {
    return state_.compositor_element_id;
  }

  const blink::ViewTransitionElementId& ViewTransitionElementId() const {
    return state_.view_transition_element_id;
  }

  const viz::ViewTransitionElementResourceId& ViewTransitionElementResourceId()
      const {
    return state_.view_transition_element_resource_id;
  }

  std::unique_ptr<JSONObject> ToJSON() const;

 private:
  EffectPaintPropertyNode(const EffectPaintPropertyNodeOrAlias* parent,
                          State&& state)
      : EffectPaintPropertyNodeOrAlias(parent), state_(std::move(state)) {}

  State state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_EFFECT_PAINT_PROPERTY_NODE_H_
