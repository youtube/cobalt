// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_

#include <algorithm>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "cc/trees/sticky_position_constraint.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_clip_cache.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper_transform_cache.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

using CompositorStickyConstraint = cc::StickyPositionConstraint;

// A transform (e.g., created by css "transform" or "perspective", or for
// internal positioning such as paint offset or scrolling) along with a
// reference to the parent TransformPaintPropertyNode. The scroll tree is
// referenced by transform nodes and a transform node with an associated scroll
// node will be a 2d transform for scroll offset.
//
// The transform tree is rooted at a node with no parent. This root node should
// not be modified.
class TransformPaintPropertyNode;

class PLATFORM_EXPORT TransformPaintPropertyNodeOrAlias
    : public PaintPropertyNode<TransformPaintPropertyNodeOrAlias,
                               TransformPaintPropertyNode> {
 public:
  // If |relative_to_node| is an ancestor of |this|, returns true if any node is
  // marked changed, at least significance of |change|, along the path from
  // |this| to |relative_to_node| (not included). Otherwise returns the combined
  // changed status of the paths from |this| and |relative_to_node| to the root.
  bool Changed(PaintPropertyChangeType change,
               const TransformPaintPropertyNodeOrAlias& relative_to_node) const;

  void AddChanged(PaintPropertyChangeType changed) {
    DCHECK_NE(PaintPropertyChangeType::kUnchanged, changed);
    GeometryMapperTransformCache::ClearCache();
    GeometryMapperClipCache::ClearCache();
    PaintPropertyNode::AddChanged(changed);
  }

 protected:
  using PaintPropertyNode::PaintPropertyNode;
};

class TransformPaintPropertyNodeAlias
    : public TransformPaintPropertyNodeOrAlias {
 public:
  static scoped_refptr<TransformPaintPropertyNodeAlias> Create(
      const TransformPaintPropertyNodeOrAlias& parent) {
    return base::AdoptRef(new TransformPaintPropertyNodeAlias(parent));
  }

 private:
  explicit TransformPaintPropertyNodeAlias(
      const TransformPaintPropertyNodeOrAlias& parent)
      : TransformPaintPropertyNodeOrAlias(parent, kParentAlias) {}
};

class PLATFORM_EXPORT TransformPaintPropertyNode
    : public TransformPaintPropertyNodeOrAlias {
 public:
  enum class BackfaceVisibility : unsigned char {
    // backface-visibility is not inherited per the css spec. However, for an
    // element that don't create a new plane, for now we let the element
    // inherit the parent backface-visibility.
    kInherited,
    // backface-visibility: hidden for the new plane.
    kHidden,
    // backface-visibility: visible for the new plane.
    kVisible,
  };

  struct PLATFORM_EXPORT TransformAndOrigin {
    gfx::Transform matrix;
    gfx::Point3F origin;
  };

  struct AnimationState {
    AnimationState() {}
    bool is_running_animation_on_compositor = false;
    STACK_ALLOCATED();
  };

  // To make it less verbose and more readable to construct and update a node,
  // a struct with default values is used to represent the state.
  struct PLATFORM_EXPORT State {
    TransformAndOrigin transform_and_origin;
    scoped_refptr<const ScrollPaintPropertyNode> scroll;
    scoped_refptr<const TransformPaintPropertyNode>
        scroll_translation_for_fixed;

    // Use bitfield packing instead of separate bools to save space.
    struct Flags {
      DISALLOW_NEW();

     public:
      bool flattens_inherited_transform : 1;
      bool in_subtree_of_page_scale : 1;
      bool animation_is_axis_aligned : 1;
      bool delegates_to_parent_for_backface : 1;
      // Set if a frame is rooted at this node.
      bool is_frame_paint_offset_translation : 1;
      bool is_for_svg_child : 1;
    } flags = {false, true, false, false, false, false};

    BackfaceVisibility backface_visibility = BackfaceVisibility::kInherited;
    unsigned rendering_context_id = 0;
    CompositingReasons direct_compositing_reasons = CompositingReason::kNone;
    CompositorElementId compositor_element_id;
    std::unique_ptr<CompositorStickyConstraint> sticky_constraint;
    std::unique_ptr<cc::AnchorScrollContainersData>
        anchor_scroll_containers_data;
    // If a visible frame is rooted at this node, this represents the element
    // ID of the containing document.
    CompositorElementId visible_frame_element_id;

    PaintPropertyChangeType ComputeTransformChange(
        const TransformAndOrigin& other,
        const AnimationState& animation_state) const;
    PaintPropertyChangeType ComputeChange(
        const State& other,
        const AnimationState& animation_state) const;

    bool UsesCompositedScrolling() const {
      return direct_compositing_reasons & CompositingReason::kOverflowScrolling;
    }
    bool RequiresCullRectExpansion() const {
      return direct_compositing_reasons &
             CompositingReason::kRequiresCullRectExpansion;
    }
  };

  // This node is really a sentinel, and does not represent a real transform
  // space.
  static const TransformPaintPropertyNode& Root();

  static scoped_refptr<TransformPaintPropertyNode> Create(
      const TransformPaintPropertyNodeOrAlias& parent,
      State&& state) {
    return base::AdoptRef(
        new TransformPaintPropertyNode(&parent, std::move(state)));
  }

  const TransformPaintPropertyNode& Unalias() const = delete;
  bool IsParentAlias() const = delete;

  PaintPropertyChangeType Update(
      const TransformPaintPropertyNodeOrAlias& parent,
      State&& state,
      const AnimationState& animation_state = AnimationState()) {
    auto parent_changed = SetParent(parent);
    auto state_changed = state_.ComputeChange(state, animation_state);
    if (state_changed != PaintPropertyChangeType::kUnchanged) {
      state_ = std::move(state);
      AddChanged(state_changed);
      Validate();
    }
    return std::max(parent_changed, state_changed);
  }

  bool IsIdentityOr2dTranslation() const {
    return state_.transform_and_origin.matrix.IsIdentityOr2dTranslation();
  }
  bool IsIdentity() const {
    return state_.transform_and_origin.matrix.IsIdentity();
  }
  // Only available when IsIdentityOr2dTranslation() is true.
  gfx::Vector2dF Get2dTranslation() const {
    DCHECK(IsIdentityOr2dTranslation());
    return state_.transform_and_origin.matrix.To2dTranslation();
  }
  const gfx::Transform& Matrix() const {
    return state_.transform_and_origin.matrix;
  }

  gfx::Transform MatrixWithOriginApplied() const {
    gfx::Transform result = Matrix();
    result.ApplyTransformOrigin(Origin().x(), Origin().y(), Origin().z());
    return result;
  }

  const gfx::Point3F& Origin() const {
    return state_.transform_and_origin.origin;
  }

  PaintPropertyChangeType DirectlyUpdateTransformAndOrigin(
      TransformAndOrigin&& transform_and_origin,
      const AnimationState& animation_state);

  // The associated scroll node, or nullptr otherwise.
  const ScrollPaintPropertyNode* ScrollNode() const {
    return state_.scroll.get();
  }

  const TransformPaintPropertyNode* ScrollTranslationForFixed() const {
    return state_.scroll_translation_for_fixed.get();
  }

  // If true, this node is translated by the viewport bounds delta, which is
  // used to keep bottom-fixed elements appear fixed to the bottom of the
  // screen in the presence of URL bar movement.
  bool IsAffectedByOuterViewportBoundsDelta() const {
    return DirectCompositingReasons() &
           CompositingReason::kAffectedByOuterViewportBoundsDelta;
  }

  // If true, this node is a descendant of the page scale transform. This is
  // important for avoiding raster during pinch-zoom (see: crbug.com/951861).
  bool IsInSubtreeOfPageScale() const {
    return state_.flags.in_subtree_of_page_scale;
  }

  const CompositorStickyConstraint* GetStickyConstraint() const {
    return state_.sticky_constraint.get();
  }

  const cc::AnchorScrollContainersData* GetAnchorScrollContainersData() const {
    return state_.anchor_scroll_containers_data.get();
  }

  // If this is a scroll offset translation (i.e., has an associated scroll
  // node), returns this. Otherwise, returns the transform node that this node
  // scrolls with respect to.
  const TransformPaintPropertyNode& NearestScrollTranslationNode() const {
    return GetTransformCache().nearest_scroll_translation();
  }

  // Returns the nearest ancestor node (including |this|) that has direct
  // compositing reasons.
  const TransformPaintPropertyNode* NearestDirectlyCompositedAncestor() const {
    return GetTransformCache().nearest_directly_composited_ancestor();
  }

  // If true, content with this transform node (or its descendant) appears in
  // the plane of its parent. This is implemented by flattening the total
  // accumulated transform from its ancestors.
  bool FlattensInheritedTransform() const {
    return state_.flags.flattens_inherited_transform;
  }

  // Returns the local BackfaceVisibility value set on this node. To be used
  // for testing only; use |BackfaceVisibilitySameAsParent()| or
  // |IsBackfaceHidden()| for production code.
  BackfaceVisibility GetBackfaceVisibilityForTesting() const {
    return state_.backface_visibility;
  }

  bool IsBackfaceHidden() const {
    return GetTransformCache().is_backface_hidden();
  }

  // Returns true if the backface visibility for this node is the same as that
  // of its parent. This will be true for the Root node.
  bool BackfaceVisibilitySameAsParent() const {
    if (IsRoot())
      return true;
    if (state_.backface_visibility == BackfaceVisibility::kInherited)
      return true;
    if (state_.backface_visibility ==
        Parent()->Unalias().state_.backface_visibility)
      return true;
    return IsBackfaceHidden() == Parent()->Unalias().IsBackfaceHidden();
  }

  // Returns true if the flattens inherited transform setting for this node is
  // the same as that of its parent. This will be true for the Root node.
  bool FlattensInheritedTransformSameAsParent() const {
    if (IsRoot())
      return true;
    return state_.flags.flattens_inherited_transform ==
           Parent()->Unalias().state_.flags.flattens_inherited_transform;
  }

  bool HasDirectCompositingReasons() const {
    return DirectCompositingReasons() != CompositingReason::kNone;
  }

  bool HasDirectCompositingReasonsOtherThan3dTransform() const {
    return DirectCompositingReasons() &
           ~(CompositingReason::k3DTransform | CompositingReason::k3DScale |
             CompositingReason::k3DRotate | CompositingReason::k3DTranslate |
             CompositingReason::kTrivial3DTransform);
  }

  bool HasActiveTransformAnimation() const {
    return state_.direct_compositing_reasons &
           (CompositingReason::kActiveTransformAnimation |
            CompositingReason::kActiveScaleAnimation |
            CompositingReason::kActiveRotateAnimation |
            CompositingReason::kActiveTranslateAnimation);
  }

  bool RequiresCompositingForFixedPosition() const {
    return DirectCompositingReasons() & CompositingReason::kFixedPosition;
  }

  bool RequiresCompositingForFixedToViewport() const {
    return DirectCompositingReasons() & CompositingReason::kUndoOverscroll;
  }

  bool RequiresCompositingForStickyPosition() const {
    return DirectCompositingReasons() & CompositingReason::kStickyPosition;
  }

  bool RequiresCompositingForAnchorScroll() const {
    return DirectCompositingReasons() & CompositingReason::kAnchorScroll;
  }

  CompositingReasons DirectCompositingReasonsForDebugging() const {
    return DirectCompositingReasons();
  }

  bool TransformAnimationIsAxisAligned() const {
    return state_.flags.animation_is_axis_aligned;
  }

  bool RequiresCompositingForRootScroller() const {
    return state_.direct_compositing_reasons & CompositingReason::kRootScroller;
  }

  bool RequiresCompositingForWillChangeTransform() const {
    return state_.direct_compositing_reasons &
           (CompositingReason::kWillChangeTransform |
            CompositingReason::kWillChangeScale |
            CompositingReason::kWillChangeRotate |
            CompositingReason::kWillChangeTranslate);
  }

  // Cull rect expansion is required if the compositing reasons hint requirement
  // of high-performance movement, to avoid frequent change of cull rect.
  bool RequiresCullRectExpansion() const {
    return state_.RequiresCullRectExpansion();
  }

  const CompositorElementId& GetCompositorElementId() const {
    return state_.compositor_element_id;
  }

  const CompositorElementId& GetVisibleFrameElementId() const {
    return state_.visible_frame_element_id;
  }

  bool IsFramePaintOffsetTranslation() const {
    return state_.flags.is_frame_paint_offset_translation;
  }

  bool DelegatesToParentForBackface() const {
    return state_.flags.delegates_to_parent_for_backface;
  }

  // Content whose transform nodes have a common rendering context ID are 3D
  // sorted. If this is 0, content will not be 3D sorted.
  unsigned RenderingContextId() const { return state_.rendering_context_id; }
  bool HasRenderingContext() const { return state_.rendering_context_id; }

  bool IsForSVGChild() const { return state_.flags.is_for_svg_child; }

  std::unique_ptr<JSONObject> ToJSON() const;

 private:
  friend class PaintPropertyNode<TransformPaintPropertyNodeOrAlias,
                                 TransformPaintPropertyNode>;

  TransformPaintPropertyNode(const TransformPaintPropertyNodeOrAlias* parent,
                             State&& state)
      : TransformPaintPropertyNodeOrAlias(parent), state_(std::move(state)) {
    Validate();
  }

  CompositingReasons DirectCompositingReasons() const {
    return state_.direct_compositing_reasons;
  }

  bool IsBackfaceHiddenInternal(bool parent_backface_hidden) const {
    if (state_.backface_visibility == BackfaceVisibility::kInherited)
      return parent_backface_hidden;
    return state_.backface_visibility == BackfaceVisibility::kHidden;
  }

  void Validate() const {
#if DCHECK_IS_ON()
    if (state_.scroll) {
      // If there is an associated scroll node, this can only be a 2d
      // translation for scroll offset.
      DCHECK(IsIdentityOr2dTranslation());
      // The scroll compositor element id should be stored on the scroll node.
      DCHECK(!state_.compositor_element_id);
    }
#endif
  }

  // For access to GetTransformCache() and SetCachedTransform.
  friend class GeometryMapper;
  friend class GeometryMapperTest;
  friend class GeometryMapperTransformCache;
  friend class GeometryMapperTransformCacheTest;
  friend class PaintPropertyTreeBuilderTest;

  const GeometryMapperTransformCache& GetTransformCache() const {
    if (!transform_cache_)
      transform_cache_ = std::make_unique<GeometryMapperTransformCache>();
    transform_cache_->UpdateIfNeeded(*this);
    return *transform_cache_;
  }
  void UpdateScreenTransform() const {
    DCHECK(transform_cache_);
    transform_cache_->UpdateScreenTransform(*this);
  }

  State state_;
  mutable std::unique_ptr<GeometryMapperTransformCache> transform_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_TRANSFORM_PAINT_PROPERTY_NODE_H_
