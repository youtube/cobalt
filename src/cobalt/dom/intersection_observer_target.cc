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

#include "cobalt/dom/intersection_observer_target.h"

#include <algorithm>

#include "cobalt/cssom/computed_style_utils.h"
#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/used_style.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/intersection_observer_entry_init.h"
#include "cobalt/dom/performance.h"
#include "cobalt/script/sequence.h"

namespace cobalt {
namespace dom {

namespace {

HTMLElement* GetContainingBlockOfHTMLElement(HTMLElement* html_element) {
  // Establish the containing block, as described in
  // http://www.w3.org/TR/CSS2/visudet.html#containing-block-details
  DCHECK(html_element->node_document());
  html_element->node_document()->DoSynchronousLayout();

  scoped_refptr<cssom::PropertyValue> position =
      html_element->computed_style()->position();

  // The containing block in which the root element lives is a rectangle called
  // the initial containing block. For continuous media, it has the dimensions
  // of the viewport and is anchored at the canvas origin; it is the page area
  // for paged media.
  if (html_element->IsRootElement()) {
    return html_element->owner_document()->document_element()->AsHTMLElement();
  }

  for (Node* ancestor_node = html_element->parent_node(); ancestor_node;
       ancestor_node = ancestor_node->parent_node()) {
    Element* ancestor_element = ancestor_node->AsElement();
    if (!ancestor_element) {
      continue;
    }
    HTMLElement* ancestor_html_element = ancestor_element->AsHTMLElement();
    if (!ancestor_html_element) {
      continue;
    }

    // If the element has 'position: absolute', the containing block is
    // established by the nearest ancestor with a 'position' of 'absolute',
    // 'relative' or 'fixed'.
    // Transformed elements also act as a containing block for all descendants.
    // https://www.w3.org/TR/css-transforms-1/#transform-rendering.
    if (position == cssom::KeywordValue::GetAbsolute() &&
        ancestor_html_element->computed_style()->position() ==
            cssom::KeywordValue::GetStatic() &&
        ancestor_html_element->computed_style()->transform() ==
            cssom::KeywordValue::GetNone()) {
      continue;
    }

    // If the element has 'position: fixed', the containing block is established
    // by the viewport in the case of continuous media or the page area in the
    // case of paged media.
    // Transformed elements also act as a containing block for all descendants.
    // https://www.w3.org/TR/css-transforms-1/#transform-rendering.
    if (position == cssom::KeywordValue::GetFixed() &&
        ancestor_html_element->computed_style()->transform() ==
            cssom::KeywordValue::GetNone()) {
      continue;
    }

    // For other elements, if the element's position is 'relative' or 'static',
    // the containing block is formed by the content edge of the nearest block
    // container ancestor box.
    return ancestor_html_element;
  }

  // If there is no such ancestor, the containing block is the initial
  // containing block.
  return html_element->owner_document()->document_element()->AsHTMLElement();
}

bool IsInContainingBlockChain(const HTMLElement* potential_containing_block,
                              HTMLElement* html_element) {
  // Walk up the containing block chain, as described in
  // http://www.w3.org/TR/CSS2/visudet.html#containing-block-details
  HTMLElement* containing_block_element =
      GetContainingBlockOfHTMLElement(html_element);
  while (containing_block_element != potential_containing_block) {
    if (!containing_block_element->parent_element()) {
      return false;
    }
    containing_block_element =
        GetContainingBlockOfHTMLElement(containing_block_element);
  }
  return true;
}

}  // namespace

IntersectionObserverTarget::IntersectionObserverTarget(Element* target_element)
    : target_element_(target_element) {}

void IntersectionObserverTarget::RegisterIntersectionObserver(
    const scoped_refptr<IntersectionObserver>& observer) {
  intersection_observer_registration_list_.AddIntersectionObserver(observer);
}

void IntersectionObserverTarget::UnregisterIntersectionObserver(
    const scoped_refptr<IntersectionObserver>& observer) {
  intersection_observer_registration_list_.RemoveIntersectionObserver(observer);
}

void IntersectionObserverTarget::UpdateIntersectionObservationsForTarget(
    const scoped_refptr<IntersectionObserver>& observer) {
  // https://www.w3.org/TR/intersection-observer/#update-intersection-observations-algo
  // Subtasks for step 2 of the "run the update intersection observations steps"
  // algorithm:
  //     1. If the intersection root is not the implicit root and target is
  //        not a descendant of the intersection root in the containing block
  //        chain, skip further processing for target.
  HTMLElement* html_target = target_element_->AsHTMLElement();
  HTMLElement* html_intersection_root = observer->root()->AsHTMLElement();
  if (!html_target || !html_intersection_root) {
    NOTREACHED();
    return;
  }

  if (html_intersection_root !=
          target_element_->owner_document()->document_element() &&
      !IsInContainingBlockChain(html_intersection_root, html_target)) {
    return;
  }

  //     2. If the intersection root is not the implicit root, and target is
  //        not in the same Document as the intersection root, skip further
  //        processing for target.
  if (html_intersection_root !=
          target_element_->owner_document()->document_element() &&
      html_intersection_root->owner_document() !=
          target_element_->owner_document()) {
    return;
  }

  //     3. Let targetRect be a DOMRectReadOnly obtained by running the
  //        getBoundingClientRect() algorithm on target.
  scoped_refptr<DOMRectReadOnly> target_rect =
      target_element_->GetBoundingClientRect();

  //     4. Let intersectionRect be the result of running the compute the
  //        intersection algorithm on target.
  scoped_refptr<DOMRectReadOnly> root_bounds = GetRootBounds(
      html_intersection_root, observer->root_margin_property_value());
  scoped_refptr<DOMRectReadOnly> intersection_rect =
      ComputeIntersectionBetweenTargetAndRoot(
          html_intersection_root, root_bounds, target_rect,
          base::WrapRefCounted(html_target));

  //     5. Let targetArea be targetRect's area.
  float target_area = target_rect->rect().size().GetArea();

  //     6. Let intersectionArea be intersectionRect's area.
  float intersection_area = intersection_rect->rect().size().GetArea();

  //     7. Let isIntersecting be true if targetRect and rootBounds intersect or
  //        are edge-adjacent, even if the intersection has zero area (because
  //        rootBounds or targetRect have zero area); otherwise, let
  //        isIntersecting be false.
  bool is_intersecting =
      intersection_rect->width() != 0 || intersection_rect->height() != 0;

  //     8. If targetArea is non-zero, let intersectionRatio be intersectionArea
  //        divided by targetArea. Otherwise, let intersectionRatio be 1 if
  //        isIntersecting is true, or 0 if isIntersecting is false.
  float intersection_ratio = is_intersecting ? 1.0f : 0.0f;
  if (target_area != 0) {
    intersection_ratio = intersection_area / target_area;
  }

  //     9. Let thresholdIndex be the index of the first entry in
  //        observer.thresholds whose value is greater than intersectionRatio,
  //        or the length of observer.thresholds if intersectionRatio is greater
  //        than or equal to the last entry in observer.thresholds.
  const script::Sequence<double>& thresholds = observer->thresholds();
  size_t threshold_index;
  for (threshold_index = 0; threshold_index < thresholds.size();
       ++threshold_index) {
    if (thresholds.at(threshold_index) > intersection_ratio) {
      break;
    }
  }

  //     10. Let intersectionObserverRegistration be the
  //         IntersectionObserverRegistration record in target's internal
  //         [[RegisteredIntersectionObservers]] slot whose observer property is
  //         equal to observer.
  IntersectionObserverRegistration* intersection_observer_registration =
      intersection_observer_registration_list_.FindRegistrationForObserver(
          observer);

  if (!intersection_observer_registration) {
    NOTREACHED();
    return;
  }

  //     11. Let previousThresholdIndex be the
  //         intersectionObserverRegistration's previousThresholdIndex property.
  int32 previous_threshold_index =
      intersection_observer_registration->previous_threshold_index();

  //     12. Let previousIsIntersecting be the
  //         intersectionObserverRegistration's previousIsIntersecting property.
  bool previous_is_intersecting =
      intersection_observer_registration->previous_is_intersecting();

  //     13. If thresholdIndex does not equal previousThresholdIndex or if
  //         isIntersecting does not equal previousIsIntersecting, queue an
  //         IntersectionObserverEntry, passing in observer, time, rootBounds,
  //         boundingClientRect, intersectionRect, isIntersecting, and target.
  if (static_cast<int32>(threshold_index) != previous_threshold_index ||
      is_intersecting != previous_is_intersecting) {
    IntersectionObserverEntryInit init_dict;
    init_dict.set_time(target_element_->owner_document()
                           ->window()
                           ->performance()
                           ->timing()
                           ->GetNavigationStartClock()
                           ->Now()
                           .InMillisecondsF());
    init_dict.set_root_bounds(root_bounds);
    init_dict.set_bounding_client_rect(target_rect);
    init_dict.set_intersection_rect(intersection_rect);
    init_dict.set_is_intersecting(is_intersecting);
    init_dict.set_intersection_ratio(intersection_ratio);
    init_dict.set_target(base::WrapRefCounted(target_element_));
    observer->QueueIntersectionObserverEntry(
        base::WrapRefCounted(new IntersectionObserverEntry(init_dict)));
  }

  //     14. Assign threshold to intersectionObserverRegistration's
  //         previousThresholdIndex property.
  intersection_observer_registration->set_previous_threshold_index(
      static_cast<int32>(threshold_index));

  //     15. Assign isIntersecting to intersectionObserverRegistration's
  //         previousIsIntersecting property.
  intersection_observer_registration->set_previous_is_intersecting(
      is_intersecting);
}

scoped_refptr<DOMRectReadOnly> IntersectionObserverTarget::GetRootBounds(
    const scoped_refptr<HTMLElement>& html_intersection_root,
    scoped_refptr<cssom::PropertyListValue> root_margin_property_value) {
  // https://www.w3.org/TR/intersection-observer/#intersectionobserver-root-intersection-rectangle
  // Rules for determining the root intersection rectangle bounds.
  LayoutBoxes* intersection_root_layout_boxes =
      html_intersection_root->layout_boxes();
  DCHECK(intersection_root_layout_boxes);

  math::RectF root_bounds_without_margins;
  // If the intersection root is the implicit root, it's the viewport's size.
  if (html_intersection_root ==
      html_intersection_root->owner_document()->document_element()) {
    root_bounds_without_margins = math::RectF(
        0.0, 0.0,
        html_intersection_root->owner_document()->viewport_size().width(),
        html_intersection_root->owner_document()->viewport_size().height());
  } else if (IsOverflowCropped(html_intersection_root->computed_style())) {
    // If the intersection root has an overflow clip, it's the element's content
    // area.
    math::Vector2dF content_edge_offset =
        intersection_root_layout_boxes->GetContentEdgeOffset();
    root_bounds_without_margins =
        math::RectF(content_edge_offset.x(), content_edge_offset.y(),
                    intersection_root_layout_boxes->GetContentEdgeWidth(),
                    intersection_root_layout_boxes->GetContentEdgeHeight());
  } else {
    // Otherwise, it's the result of running the getBoundingClientRect()
    // algorithm on the intersection root.
    root_bounds_without_margins =
        math::RectF(html_intersection_root->GetBoundingClientRect()->rect());
  }
  int32 top_margin = GetUsedLengthOfRootMarginPropertyValue(
      root_margin_property_value->value()[0],
      root_bounds_without_margins.height());
  int32 right_margin = GetUsedLengthOfRootMarginPropertyValue(
      root_margin_property_value->value()[1],
      root_bounds_without_margins.width());
  int32 bottom_margin = GetUsedLengthOfRootMarginPropertyValue(
      root_margin_property_value->value()[2],
      root_bounds_without_margins.height());
  int32 left_margin = GetUsedLengthOfRootMarginPropertyValue(
      root_margin_property_value->value()[3],
      root_bounds_without_margins.width());

  // Remember to grow or shrink the root intersection rectangle bounds based
  // on the root margin property.
  scoped_refptr<DOMRectReadOnly> root_bounds = new DOMRectReadOnly(
      root_bounds_without_margins.x() - left_margin,
      root_bounds_without_margins.y() - top_margin,
      root_bounds_without_margins.width() + left_margin + right_margin,
      root_bounds_without_margins.height() + top_margin + bottom_margin);
  return root_bounds;
}

int32 IntersectionObserverTarget::GetUsedLengthOfRootMarginPropertyValue(
    const scoped_refptr<cssom::PropertyValue>& length_property_value,
    float percentage_base) {
  cssom::UsedLengthValueProvider<float> used_length_provider(percentage_base);
  length_property_value->Accept(&used_length_provider);
  // Not explicitly stated in web spec, but has been observed that Chrome
  // truncates root margin decimal values
  return static_cast<int32>(used_length_provider.used_length().value_or(0.0f));
}

scoped_refptr<DOMRectReadOnly>
IntersectionObserverTarget::ComputeIntersectionBetweenTargetAndRoot(
    const scoped_refptr<HTMLElement>& html_intersection_root,
    const scoped_refptr<DOMRectReadOnly>& root_bounds,
    const scoped_refptr<DOMRectReadOnly>& target_rect,
    const scoped_refptr<HTMLElement>& html_target) {
  // https://www.w3.org/TR/intersection-observer/#calculate-intersection-rect-algo
  // To compute the intersection between a target and the observer's
  // intersection root, run these steps:
  // 1. Let intersectionRect be the result of running the
  // getBoundingClientRect() algorithm on the target.
  math::RectF intersection_rect = target_rect->rect();

  // 2. Let container be the containing block of the target.
  LayoutBoxes* target_layout_boxes = html_target->layout_boxes();
  DCHECK(target_layout_boxes);
  math::Vector2dF total_offset_from_containing_block =
      target_layout_boxes->GetBorderEdgeOffsetFromContainingBlock();

  HTMLElement* prev_container = html_target;
  HTMLElement* container = GetContainingBlockOfHTMLElement(prev_container);

  // 3. While container is not the intersection root:
  while (container != html_intersection_root) {
    //     1. Map intersectionRect to the coordinate space of container.
    intersection_rect.set_x(total_offset_from_containing_block.x());
    intersection_rect.set_y(total_offset_from_containing_block.y());

    //     2. If container has overflow clipping or a css clip-path property,
    //     update intersectionRect by applying container's clip.
    //     (Note: The containing block of an element with 'position: absolute'
    //     is formed by the padding edge of the ancestor.
    //     https://www.w3.org/TR/CSS2/visudet.html)
    LayoutBoxes* container_layout_boxes = container->layout_boxes();
    DCHECK(container_layout_boxes);
    if (IsOverflowCropped(container->computed_style())) {
      math::Vector2dF container_clip_dimensions =
          prev_container->computed_style()->position() ==
                  cssom::KeywordValue::GetAbsolute()
              ? math::Vector2dF(container_layout_boxes->GetPaddingEdgeWidth(),
                                container_layout_boxes->GetPaddingEdgeHeight())
              : math::Vector2dF(container_layout_boxes->GetContentEdgeWidth(),
                                container_layout_boxes->GetContentEdgeHeight());
      math::RectF container_clip(0, 0, container_clip_dimensions.x(),
                                 container_clip_dimensions.y());
      intersection_rect =
          IntersectIntersectionObserverRects(intersection_rect, container_clip);
    }

    //     3. If container is the root element of a nested browsing context,
    //     update container to be the browsing context container of container,
    //     and update intersectionRect by clipping to the viewport of the nested
    //     browsing context. Otherwise, update container to be the containing
    //     block of container.
    //     (Note: The containing block of an element with 'position: absolute'
    //     is formed by the padding edge of the ancestor.
    //     https://www.w3.org/TR/CSS2/visudet.html)
    math::Vector2dF next_offset_from_containing_block =
        prev_container->computed_style()->position() ==
                cssom::KeywordValue::GetAbsolute()
            ? container_layout_boxes->GetPaddingEdgeOffsetFromContainingBlock()
            : container_layout_boxes->GetContentEdgeOffsetFromContainingBlock();
    total_offset_from_containing_block += next_offset_from_containing_block;

    prev_container = container;
    container = GetContainingBlockOfHTMLElement(prev_container);
  }

  // Modification of steps 4-6:
  // Map intersectionRect to the coordinate space of the viewport of the
  // Document containing the target.
  // (Note: The containing block of an element with 'position: absolute'
  // is formed by the padding edge of the ancestor.
  // https://www.w3.org/TR/CSS2/visudet.html)
  LayoutBoxes* container_layout_boxes = container->layout_boxes();
  DCHECK(container_layout_boxes);
  math::Vector2dF containing_block_offset_from_origin =
      prev_container->computed_style()->position() ==
              cssom::KeywordValue::GetAbsolute()
          ? container_layout_boxes->GetPaddingEdgeOffset()
          : container_layout_boxes->GetContentEdgeOffset();

  intersection_rect.set_x(total_offset_from_containing_block.x() +
                          containing_block_offset_from_origin.x());
  intersection_rect.set_y(total_offset_from_containing_block.y() +
                          containing_block_offset_from_origin.y());

  // Update intersectionRect by intersecting it with the root intersection
  // rectangle, which is already in this coordinate space.
  intersection_rect = IntersectIntersectionObserverRects(intersection_rect,
                                                         root_bounds->rect());

  return base::WrapRefCounted(new DOMRectReadOnly(intersection_rect));
}

math::RectF IntersectionObserverTarget::IntersectIntersectionObserverRects(
    const math::RectF& a, const math::RectF& b) {
  float rx = std::max(a.x(), b.x());
  float ry = std::max(a.y(), b.y());
  float rr = std::min(a.right(), b.right());
  float rb = std::min(a.bottom(), b.bottom());

  if (rx > rr || ry > rb) {
    return math::RectF(0.0f, 0.0f, 0.0f, 0.0f);
  }

  return math::RectF(rx, ry, rr - rx, rb - ry);
}

}  // namespace dom
}  // namespace cobalt
