/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"

#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/svg_container_painter.h"

namespace blink {

LayoutSVGContainer::LayoutSVGContainer(SVGElement* node)
    : LayoutSVGModelObject(node),
      object_bounding_box_valid_(false),
      needs_boundaries_update_(true),
      did_screen_scale_factor_change_(false),
      has_non_isolated_blending_descendants_(false),
      has_non_isolated_blending_descendants_dirty_(false) {}

LayoutSVGContainer::~LayoutSVGContainer() = default;

void LayoutSVGContainer::Trace(Visitor* visitor) const {
  visitor->Trace(content_);
  LayoutSVGModelObject::Trace(visitor);
}

void LayoutSVGContainer::UpdateLayout() {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  // Update the local transform in subclasses.
  // At this point our bounding box may be incorrect, so any box relative
  // transforms will be incorrect. Since descendants only require the scaling
  // components to be correct, this should be fine. We update the transform
  // again, if needed, after computing the bounding box below.
  SVGTransformChange transform_change = CalculateLocalTransform(false);
  did_screen_scale_factor_change_ =
      transform_change == SVGTransformChange::kFull ||
      SVGLayoutSupport::ScreenScaleFactorChanged(Parent());

  SVGContainerLayoutInfo layout_info;
  layout_info.scale_factor_changed = did_screen_scale_factor_change_;
  // When HasRelativeLengths() is false, no descendants have relative lengths
  // (hence no one is interested in viewport size changes).
  layout_info.viewport_changed =
      GetElement()->HasRelativeLengths() &&
      SVGLayoutSupport::LayoutSizeOfNearestViewportChanged(this);

  content_.Layout(layout_info);

  bool bbox_changed = false;
  if (needs_boundaries_update_) {
    bbox_changed = UpdateCachedBoundaries();
    needs_boundaries_update_ = false;
  }

  // Invalidate all resources of this client if our reference box changed.
  if (EverHadLayout() && (SelfNeedsLayout() || bbox_changed))
    SVGResourceInvalidator(*this).InvalidateEffects();

  if (transform_change != SVGTransformChange::kNone || bbox_changed) {
    CalculateLocalTransform(bbox_changed);

    // If our bounds or transform changed, notify the parents.
    LayoutSVGModelObject::SetNeedsBoundariesUpdate();
  }

  if (!IsSVGHiddenContainer()) {
    SetTransformAffectsVectorEffect(false);
    ClearSVGDescendantMayHaveTransformRelatedAnimation();
    for (auto* child = FirstChild(); child; child = child->NextSibling()) {
      if (child->TransformAffectsVectorEffect())
        SetTransformAffectsVectorEffect(true);
      if (child->StyleRef().HasCurrentTransformRelatedAnimation() ||
          child->SVGDescendantMayHaveTransformRelatedAnimation()) {
        SetSVGDescendantMayHaveTransformRelatedAnimation();
      }
    }
  }

  DCHECK(!needs_boundaries_update_);
  ClearNeedsLayout();
}

void LayoutSVGContainer::AddChild(LayoutObject* child,
                                  LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutSVGModelObject::AddChild(child, before_child);

  bool should_isolate_descendants =
      (child->IsBlendingAllowed() && child->StyleRef().HasBlendMode()) ||
      child->HasNonIsolatedBlendingDescendants();
  if (should_isolate_descendants)
    DescendantIsolationRequirementsChanged(kDescendantIsolationRequired);
}

void LayoutSVGContainer::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  LayoutSVGModelObject::RemoveChild(child);

  bool had_non_isolated_descendants =
      (child->IsBlendingAllowed() && child->StyleRef().HasBlendMode()) ||
      child->HasNonIsolatedBlendingDescendants();
  if (had_non_isolated_descendants)
    DescendantIsolationRequirementsChanged(kDescendantIsolationNeedsUpdate);
}

void LayoutSVGContainer::StyleDidChange(StyleDifference diff,
                                        const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGModelObject::StyleDidChange(diff, old_style);

  bool had_isolation =
      old_style && !IsSVGHiddenContainer() &&
      SVGLayoutSupport::WillIsolateBlendingDescendantsForStyle(*old_style);

  bool will_isolate_blending_descendants =
      SVGLayoutSupport::WillIsolateBlendingDescendantsForObject(this);

  bool isolation_changed = had_isolation != will_isolate_blending_descendants;

  if (isolation_changed)
    SetNeedsPaintPropertyUpdate();

  if (!Parent() || !isolation_changed)
    return;

  if (HasNonIsolatedBlendingDescendants()) {
    Parent()->DescendantIsolationRequirementsChanged(
        will_isolate_blending_descendants ? kDescendantIsolationNeedsUpdate
                                          : kDescendantIsolationRequired);
  }
}

bool LayoutSVGContainer::HasNonIsolatedBlendingDescendants() const {
  NOT_DESTROYED();
  if (has_non_isolated_blending_descendants_dirty_) {
    has_non_isolated_blending_descendants_ =
        content_.ComputeHasNonIsolatedBlendingDescendants();
    has_non_isolated_blending_descendants_dirty_ = false;
  }
  return has_non_isolated_blending_descendants_;
}

void LayoutSVGContainer::DescendantIsolationRequirementsChanged(
    DescendantIsolationState state) {
  NOT_DESTROYED();
  switch (state) {
    case kDescendantIsolationRequired:
      has_non_isolated_blending_descendants_ = true;
      has_non_isolated_blending_descendants_dirty_ = false;
      break;
    case kDescendantIsolationNeedsUpdate:
      if (has_non_isolated_blending_descendants_dirty_)
        return;
      has_non_isolated_blending_descendants_dirty_ = true;
      break;
  }
  if (SVGLayoutSupport::WillIsolateBlendingDescendantsForObject(this)) {
    SetNeedsPaintPropertyUpdate();
    return;
  }
  if (Parent())
    Parent()->DescendantIsolationRequirementsChanged(state);
}

void LayoutSVGContainer::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  SVGContainerPainter(*this).Paint(paint_info);
}

bool LayoutSVGContainer::UpdateCachedBoundaries() {
  NOT_DESTROYED();
  return content_.UpdateBoundingBoxes(object_bounding_box_valid_);
}

bool LayoutSVGContainer::NodeAtPoint(HitTestResult& result,
                                     const HitTestLocation& hit_test_location,
                                     const PhysicalOffset& accumulated_offset,
                                     HitTestPhase phase) {
  NOT_DESTROYED();
  DCHECK_EQ(accumulated_offset, PhysicalOffset());
  TransformedHitTestLocation local_location(hit_test_location,
                                            LocalToSVGParentTransform());
  if (!local_location)
    return false;
  if (!SVGLayoutSupport::IntersectsClipPath(*this, content_.ObjectBoundingBox(),
                                            *local_location))
    return false;

  if (!ChildPaintBlockedByDisplayLock() &&
      content_.HitTest(result, *local_location, phase))
    return true;

  // pointer-events: bounding-box makes it possible for containers to be direct
  // targets.
  if (StyleRef().UsedPointerEvents() == EPointerEvents::kBoundingBox) {
    // Check for a valid bounding box because it will be invalid for empty
    // containers.
    if (IsObjectBoundingBoxValid() &&
        local_location->Intersects(ObjectBoundingBox())) {
      UpdateHitTestResult(result, PhysicalOffset::FromPointFRound(
                                      local_location->TransformedPoint()));
      if (result.AddNodeToListBasedTestResult(GetElement(), *local_location) ==
          kStopHitTesting)
        return true;
    }
  }
  // 16.4: "If there are no graphics elements whose relevant graphics content is
  // under the pointer (i.e., there is no target element), the event is not
  // dispatched."
  return false;
}

SVGTransformChange LayoutSVGContainer::CalculateLocalTransform(
    bool bounds_changed) {
  NOT_DESTROYED();
  return SVGTransformChange::kNone;
}

}  // namespace blink
