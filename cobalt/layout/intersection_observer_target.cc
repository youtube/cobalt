// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/layout/intersection_observer_target.h"

#include <algorithm>
#include <vector>

#include "base/trace_event/trace_event.h"
#include "cobalt/cssom/computed_style_utils.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/container_box.h"
#include "cobalt/layout/intersection_observer_root.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {
namespace {

int32 GetUsedLengthOfRootMarginPropertyValue(
    const scoped_refptr<cssom::PropertyValue>& length_property_value,
    LayoutUnit percentage_base) {
  UsedLengthValueProvider used_length_provider(percentage_base);
  length_property_value->Accept(&used_length_provider);
  // Not explicitly stated in web spec, but has been observed that Chrome
  // truncates root margin decimal values.
  return static_cast<int32>(
      used_length_provider.used_length().value_or(LayoutUnit(0.0f)).toFloat());
}

// Rules for determining the root intersection rectangle bounds.
// https://www.w3.org/TR/intersection-observer/#intersectionobserver-root-intersection-rectangle
math::RectF GetRootBounds(
    const ContainerBox* root_box,
    scoped_refptr<cssom::PropertyListValue> root_margin_property_value) {
  math::RectF root_bounds_without_margins;
  if (IsOverflowCropped(root_box->computed_style())) {
    // If the intersection root has an overflow clip, it's the element's content
    // area.
    Vector2dLayoutUnit content_edge_offset =
        root_box->GetContentBoxOffsetFromRoot(false /*transform_forms_root*/);
    root_bounds_without_margins = math::RectF(
        content_edge_offset.x().toFloat(), content_edge_offset.y().toFloat(),
        root_box->width().toFloat(), root_box->height().toFloat());
  } else {
    // Otherwise, it's the result of running the getBoundingClientRect()
    // algorithm on the intersection root.
    RectLayoutUnit root_transformed_border_box(
        root_box->GetTransformedBoxFromRoot(
            root_box->GetBorderBoxFromMarginBox()));
    root_bounds_without_margins =
        math::RectF(root_transformed_border_box.x().toFloat(),
                    root_transformed_border_box.y().toFloat(),
                    root_transformed_border_box.width().toFloat(),
                    root_transformed_border_box.height().toFloat());
  }
  int32 top_margin = GetUsedLengthOfRootMarginPropertyValue(
      root_margin_property_value->value()[0],
      LayoutUnit(root_bounds_without_margins.height()));
  int32 right_margin = GetUsedLengthOfRootMarginPropertyValue(
      root_margin_property_value->value()[1],
      LayoutUnit(root_bounds_without_margins.width()));
  int32 bottom_margin = GetUsedLengthOfRootMarginPropertyValue(
      root_margin_property_value->value()[2],
      LayoutUnit(root_bounds_without_margins.height()));
  int32 left_margin = GetUsedLengthOfRootMarginPropertyValue(
      root_margin_property_value->value()[3],
      LayoutUnit(root_bounds_without_margins.width()));

  // Remember to grow or shrink the root intersection rectangle bounds based
  // on the root margin property.
  math::RectF root_bounds = math::RectF(
      root_bounds_without_margins.x() - left_margin,
      root_bounds_without_margins.y() - top_margin,
      root_bounds_without_margins.width() + left_margin + right_margin,
      root_bounds_without_margins.height() + top_margin + bottom_margin);
  return root_bounds;
}

// Similar to the IntersectRects function in math::RectF, but handles edge
// adjacent intersections as valid intersections (instead of returning a
// rectangle with zero dimensions)
math::RectF IntersectIntersectionObserverRects(const math::RectF& a,
                                               const math::RectF& b) {
  float rx = std::max(a.x(), b.x());
  float ry = std::max(a.y(), b.y());
  float rr = std::min(a.right(), b.right());
  float rb = std::min(a.bottom(), b.bottom());

  if (rx > rr || ry > rb) {
    return math::RectF(0.0f, 0.0f, 0.0f, 0.0f);
  }

  return math::RectF(rx, ry, rr - rx, rb - ry);
}

// Compute the intersection between a target and the observer's intersection
// root.
// https://www.w3.org/TR/intersection-observer/#calculate-intersection-rect-algo
math::RectF ComputeIntersectionBetweenTargetAndRoot(
    const ContainerBox* root_box, const math::RectF& root_bounds,
    const math::RectF& target_rect, const ContainerBox* target_box) {
  // Let intersectionRect be target's bounding border box.
  math::RectF intersection_rect = target_rect;

  // Let container be the containing block of the target.
  const ContainerBox* prev_container = target_box;
  const ContainerBox* container = prev_container->GetContainingBlock();
  RectLayoutUnit box_from_containing_block =
      target_box->GetTransformedBoxFromContainingBlock(
          container, target_box->GetBorderBoxFromMarginBox());
  math::Vector2dF total_offset_from_containing_block =
      math::Vector2dF(box_from_containing_block.x().toFloat(),
                      box_from_containing_block.y().toFloat());

  // While container is not the intersection root:
  while (container != root_box) {
    // Map intersectionRect to the coordinate space of container.
    intersection_rect.set_x(total_offset_from_containing_block.x());
    intersection_rect.set_y(total_offset_from_containing_block.y());

    // If container has overflow clipping or a css clip-path property, update
    // intersectionRect by applying container's clip. (Note: The containing
    // block of an element with 'position: absolute' is formed by the padding
    // edge of the ancestor. https://www.w3.org/TR/CSS2/visudet.html)
    if (IsOverflowCropped(container->computed_style())) {
      Vector2dLayoutUnit container_clip_dimensions =
          prev_container->computed_style()->position() ==
                  cssom::KeywordValue::GetAbsolute()
              ? Vector2dLayoutUnit(container->GetPaddingBoxWidth(),
                                   container->GetPaddingBoxHeight())
              : Vector2dLayoutUnit(container->width(), container->height());
      math::RectF container_clip(0.0f, 0.0f,
                                 container_clip_dimensions.x().toFloat(),
                                 container_clip_dimensions.y().toFloat());
      intersection_rect =
          IntersectIntersectionObserverRects(intersection_rect, container_clip);
    }

    // If container is the root element of a nested browsing context, update
    // container to be the browsing context container of container, and update
    // intersectionRect by clipping to the viewport of the nested browsing
    // context. Otherwise, update container to be the containing block of
    // container. (Note: The containing block of an element with 'position:
    // absolute' is formed by the padding edge of the ancestor.
    // https://www.w3.org/TR/CSS2/visudet.html)
    RectLayoutUnit next_box_from_containing_block =
        prev_container->computed_style()->position() ==
                cssom::KeywordValue::GetAbsolute()
            ? container->GetTransformedBoxFromContainingBlock(
                  container->GetContainingBlock(),
                  container->GetPaddingBoxFromMarginBox())
            : container->GetTransformedBoxFromContainingBlockContentBox(
                  container->GetContainingBlock(),
                  container->GetContentBoxFromMarginBox());
    math::Vector2dF next_offset_from_containing_block =
        math::Vector2dF(next_box_from_containing_block.x().toFloat(),
                        next_box_from_containing_block.y().toFloat());
    total_offset_from_containing_block += next_offset_from_containing_block;

    prev_container = container;
    container = prev_container->GetContainingBlock();
  }

  // Map intersectionRect to the coordinate space of the viewport of the
  // Document containing the target.
  // (Note: The containing block of an element with 'position: absolute'
  // is formed by the padding edge of the ancestor.
  // https://www.w3.org/TR/CSS2/visudet.html)
  RectLayoutUnit containing_block_box_from_origin =
      prev_container->computed_style()->position() ==
                  cssom::KeywordValue::GetAbsolute() &&
              !IsOverflowCropped(container->computed_style())
          ? container->GetTransformedBoxFromRoot(
                container->GetPaddingBoxFromMarginBox())
          : container->GetTransformedBoxFromRoot(
                container->GetContentBoxFromMarginBox());
  math::Vector2dF containing_block_offset_from_origin =
      math::Vector2dF(containing_block_box_from_origin.x().toFloat(),
                      containing_block_box_from_origin.y().toFloat());

  intersection_rect.set_x(total_offset_from_containing_block.x() +
                          containing_block_offset_from_origin.x());
  intersection_rect.set_y(total_offset_from_containing_block.y() +
                          containing_block_offset_from_origin.y());

  // Update intersectionRect by intersecting it with the root intersection
  // rectangle, which is already in this coordinate space.
  intersection_rect =
      IntersectIntersectionObserverRects(intersection_rect, root_bounds);
  return intersection_rect;
}

}  // namespace

void IntersectionObserverTarget::UpdateIntersectionObservationsForTarget(
    ContainerBox* target_box) {
  TRACE_EVENT0(
      "cobalt::layout",
      "IntersectionObserverTarget::UpdateIntersectionObservationsForTarget()");
  // Walk up the containing block chain looking for the box referencing the
  // IntersectionObserverRoot corresponding to this IntersectionObserverTarget.
  // Skip further processing for the target if it is not a descendant of the
  // root in the containing block chain.
  const ContainerBox* root_box = target_box->GetContainingBlock();
  while (!root_box->ContainsIntersectionObserverRoot(
      intersection_observer_root_)) {
    if (!root_box->parent()) {
      return;
    }
    root_box = root_box->GetContainingBlock();
  }

  // Let targetRect be target's bounding border box.
  RectLayoutUnit target_transformed_border_box(
      target_box->GetTransformedBoxFromRoot(
          target_box->GetBorderBoxFromMarginBox()));
  const math::RectF target_rect =
      math::RectF(target_transformed_border_box.x().toFloat(),
                  target_transformed_border_box.y().toFloat(),
                  target_transformed_border_box.width().toFloat(),
                  target_transformed_border_box.height().toFloat());

  // Let intersectionRect be the result of running the compute the intersection
  // algorithm on target.
  const math::RectF root_bounds = GetRootBounds(
      root_box, intersection_observer_root_->root_margin_property_value());
  const math::RectF intersection_rect = ComputeIntersectionBetweenTargetAndRoot(
      root_box, root_bounds, target_rect, target_box);

  // Let targetArea be targetRect's area.
  float target_area = target_rect.size().GetArea();

  // Let intersectionArea be intersectionRect's area.
  float intersection_area = intersection_rect.size().GetArea();

  // Let isIntersecting be true if targetRect and rootBounds intersect or are
  // edge-adjacent, even if the intersection has zero area (because rootBounds
  // or targetRect have zero area); otherwise, let isIntersecting be false.
  bool is_intersecting =
      intersection_rect.width() != 0 || intersection_rect.height() != 0 ||
      (target_rect.width() == 0 && target_rect.height() == 0 &&
       root_bounds.Contains(target_rect));

  // If targetArea is non-zero, let intersectionRatio be intersectionArea
  // divided by targetArea. Otherwise, let intersectionRatio be 1 if
  // isIntersecting is true, or 0 if isIntersecting is false.
  float intersection_ratio = target_area > 0 ? intersection_area / target_area
                                             : is_intersecting ? 1.0f : 0.0f;

  // Let thresholdIndex be the index of the first entry in observer.thresholds
  // whose value is greater than intersectionRatio, or the length of
  // observer.thresholds if intersectionRatio is greater than or equal to the
  // last entry in observer.thresholds.
  const std::vector<double>& thresholds =
      intersection_observer_root_->thresholds_vector();
  size_t threshold_index;
  for (threshold_index = 0; threshold_index < thresholds.size();
       ++threshold_index) {
    if (thresholds.at(threshold_index) > intersection_ratio) {
      // isIntersecting is false if intersectionRatio is less than all
      // thresholds, sorted ascending. Not in spec but follows Chrome behavior.
      if (threshold_index == 0) {
        is_intersecting = false;
      }
      break;
    }
  }

  // If thresholdIndex does not equal previousThresholdIndex or if
  // isIntersecting does not equal previousIsIntersecting, queue an
  // IntersectionObserverEntry, passing in observer, time, rootBounds,
  //         boundingClientRect, intersectionRect, isIntersecting, and target.
  if (static_cast<int32>(threshold_index) != previous_threshold_index_ ||
      is_intersecting != previous_is_intersecting_) {
    on_intersection_callback_.Run(root_bounds, target_rect, intersection_rect,
                                  is_intersecting, intersection_ratio);
  }

  // Update the previousThresholdIndex and previousIsIntersecting properties.
  previous_threshold_index_ = static_cast<int32>(threshold_index);
  previous_is_intersecting_ = is_intersecting;
}

}  // namespace layout
}  // namespace cobalt
