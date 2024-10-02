/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"

#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"

namespace blink {

LayoutSVGViewportContainer::LayoutSVGViewportContainer(SVGSVGElement* node)
    : LayoutSVGContainer(node),
      is_layout_size_changed_(false),
      needs_transform_update_(true) {}

void LayoutSVGViewportContainer::UpdateLayout() {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  const auto* svg = To<SVGSVGElement>(GetElement());
  is_layout_size_changed_ = SelfNeedsLayout() && svg->HasRelativeLengths();

  if (SelfNeedsLayout()) {
    SVGLengthContext length_context(svg);
    gfx::RectF old_viewport = viewport_;
    viewport_.SetRect(svg->x()->CurrentValue()->Value(length_context),
                      svg->y()->CurrentValue()->Value(length_context),
                      svg->width()->CurrentValue()->Value(length_context),
                      svg->height()->CurrentValue()->Value(length_context));
    if (old_viewport != viewport_) {
      SetNeedsBoundariesUpdate();
      // The transform depends on viewport values.
      SetNeedsTransformUpdate();
    }
  }

  LayoutSVGContainer::UpdateLayout();
}

void LayoutSVGViewportContainer::SetNeedsTransformUpdate() {
  NOT_DESTROYED();
  // The transform paint property relies on the SVG transform being up-to-date
  // (see: PaintPropertyTreeBuilder::updateTransformForNonRootSVG).
  SetNeedsPaintPropertyUpdate();
  needs_transform_update_ = true;
}

SVGTransformChange LayoutSVGViewportContainer::CalculateLocalTransform(
    bool bounds_changed) {
  NOT_DESTROYED();
  if (!needs_transform_update_)
    return SVGTransformChange::kNone;

  const auto* svg = To<SVGSVGElement>(GetElement());
  SVGTransformChangeDetector change_detector(local_to_parent_transform_);
  local_to_parent_transform_ =
      AffineTransform::Translation(viewport_.x(), viewport_.y()) *
      svg->ViewBoxToViewTransform(viewport_.size());
  needs_transform_update_ = false;
  return change_detector.ComputeChange(local_to_parent_transform_);
}

bool LayoutSVGViewportContainer::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestPhase phase) {
  NOT_DESTROYED();
  // Respect the viewport clip which is in parent coordinates.
  if (SVGLayoutSupport::IsOverflowHidden(*this)) {
    if (!hit_test_location.Intersects(viewport_))
      return false;
  }
  return LayoutSVGContainer::NodeAtPoint(result, hit_test_location,
                                         accumulated_offset, phase);
}

void LayoutSVGViewportContainer::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGContainer::StyleDidChange(diff, old_style);

  if (old_style && (SVGLayoutSupport::IsOverflowHidden(*old_style) !=
                    SVGLayoutSupport::IsOverflowHidden(StyleRef()))) {
    // See NeedsOverflowClip() in PaintPropertyTreeBuilder for the reason.
    SetNeedsPaintPropertyUpdate();
  }
}

}  // namespace blink
