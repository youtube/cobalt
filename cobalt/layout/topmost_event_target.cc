// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/layout/topmost_event_target.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/computed_style_utils.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/lottie_player.h"
#include "cobalt/dom/mouse_event.h"
#include "cobalt/dom/mouse_event_init.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/dom/pointer_event_init.h"
#include "cobalt/dom/pointer_state.h"
#include "cobalt/dom/ui_event.h"
#include "cobalt/dom/wheel_event.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/vector2d.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/web/event.h"

namespace cobalt {
namespace layout {

scoped_refptr<dom::HTMLElement> TopmostEventTarget::FindTopmostEventTarget(
    const scoped_refptr<dom::Document>& document,
    const math::Vector2dF& coordinate) {
  TRACE_EVENT0("cobalt::layout",
               "TopmostEventTarget::FindTopmostEventTarget()");
  DCHECK(document);
  DCHECK(!box_);
  DCHECK(render_sequence_.empty());

  // Make sure the document's layout box tree is up-to-date.
  document->DoSynchronousLayout();

  html_element_ = document->html();
  bool consider_only_fixed_elements = false;
  ConsiderElement(html_element_, coordinate, consider_only_fixed_elements);
  box_ = NULL;
  render_sequence_.clear();
  scoped_refptr<dom::HTMLElement> topmost_element;
  topmost_element.swap(html_element_);
  DCHECK(!html_element_);
  return topmost_element;
}

namespace {

LayoutBoxes* GetLayoutBoxesIfNotEmpty(dom::Element* element) {
  dom::HTMLElement* html_element = element->AsHTMLElement();
  if (html_element && html_element->computed_style()) {
    dom::LayoutBoxes* dom_layout_boxes = html_element->layout_boxes();
    if (dom_layout_boxes &&
        dom_layout_boxes->type() == dom::LayoutBoxes::kLayoutLayoutBoxes) {
      LayoutBoxes* layout_boxes =
          base::polymorphic_downcast<LayoutBoxes*>(dom_layout_boxes);
      if (!layout_boxes->boxes().empty()) {
        return layout_boxes;
      }
    }
  }
  return NULL;
}

std::unique_ptr<dom::PossibleScrollTargets> FindPossibleScrollTargets(
    scoped_refptr<dom::Element> target_element) {
  std::unique_ptr<dom::PossibleScrollTargets> possible_scroll_targets =
      std::make_unique<dom::PossibleScrollTargets>();

  for (auto current_element = target_element; !!current_element;
       current_element = current_element->parent_element()) {
    auto current_html_element = current_element->AsHTMLElement();
    if (!current_html_element) {
      continue;
    }

    bool pointer_events_enabled =
        current_html_element->computed_style()->pointer_events() !=
        cssom::KeywordValue::GetNone();
    if (!pointer_events_enabled) {
      continue;
    }

    auto current_ui_nav_item = current_html_element->GetUiNavItem();
    if (!current_ui_nav_item) {
      continue;
    }

    float scroll_top_lower_bound;
    float scroll_left_lower_bound;
    float scroll_top_upper_bound;
    float scroll_left_upper_bound;
    current_ui_nav_item->GetBounds(
        &scroll_top_lower_bound, &scroll_left_lower_bound,
        &scroll_top_upper_bound, &scroll_left_upper_bound);

    float offset_x;
    float offset_y;
    current_ui_nav_item->GetContentOffset(&offset_x, &offset_y);

    bool can_scroll_left = scroll_left_lower_bound < offset_x;
    bool can_scroll_right = scroll_left_upper_bound > offset_x;
    bool can_scroll_up = scroll_top_lower_bound < offset_y;
    bool can_scroll_down = scroll_top_upper_bound > offset_y;

    if (can_scroll_left && !possible_scroll_targets->left) {
      possible_scroll_targets->left = current_html_element;
    }
    if (can_scroll_right && !possible_scroll_targets->right) {
      possible_scroll_targets->right = current_html_element;
    }
    if (can_scroll_up && !possible_scroll_targets->up) {
      possible_scroll_targets->up = current_html_element;
    }
    if (can_scroll_down && !possible_scroll_targets->down) {
      possible_scroll_targets->down = current_html_element;
    }
  }
  return possible_scroll_targets;
}

scoped_refptr<dom::HTMLElement> FindFirstElementWithScrollType(
    dom::PossibleScrollTargets* possible_scroll_targets,
    ui_navigation::scroll_engine::ScrollType major_scroll_axis,
    bool scrolling_right, bool scrolling_down) {
  bool scrolling_left = !scrolling_right;
  bool scrolling_up = !scrolling_down;
  bool horizontal_scroll_axis =
      major_scroll_axis == ui_navigation::scroll_engine::ScrollType::Horizontal;
  bool vertical_scroll_axis =
      major_scroll_axis == ui_navigation::scroll_engine::ScrollType::Vertical;

  if (scrolling_left && horizontal_scroll_axis) {
    return possible_scroll_targets->left;
  } else if (scrolling_right && horizontal_scroll_axis) {
    return possible_scroll_targets->right;
  } else if (scrolling_up && vertical_scroll_axis) {
    return possible_scroll_targets->up;
  } else if (scrolling_down && vertical_scroll_axis) {
    return possible_scroll_targets->down;
  }
  return nullptr;
}

bool TransformCanBeAppliedToBox(const Box* box, math::Vector2dF* coordinate) {
  return !box->IsTransformed() ||
         box->ApplyTransformActionToCoordinate(coordinate);
}

struct CanTargetBox {
  const math::Vector2dF* coordinate;
  explicit CanTargetBox(const math::Vector2dF* coordinate)
      : coordinate(coordinate) {}
  bool operator()(const scoped_refptr<Box> box) const {
    return box->CoordinateCanTarget(coordinate);
  }
};

bool CanTargetElementAndChildren(dom::Element* element,
                                 math::Vector2dF* coordinate) {
  LayoutBoxes* layout_boxes = GetLayoutBoxesIfNotEmpty(element);
  const Boxes boxes = layout_boxes->boxes();
  const Box* box = boxes.front();
  if (!box->computed_style()) {
    return true;
  }

  return TransformCanBeAppliedToBox(box, coordinate) &&
         std::any_of(boxes.begin(), boxes.end(), CanTargetBox(coordinate));
}

// Return the nearest common ancestor of previous_element and target_element
scoped_refptr<dom::Element> GetNearestCommonAncestor(
    scoped_refptr<dom::HTMLElement> previous_element,
    scoped_refptr<dom::HTMLElement> target_element) {
  scoped_refptr<dom::Element> nearest_common_ancestor;
  if (previous_element == target_element) {
    nearest_common_ancestor = target_element;
  } else {
    if (previous_element && target_element) {
      // Find the nearest common ancestor, if there is any.
      dom::Document* previous_document = previous_element->node_document();
      // The elements only have a common ancestor if they are both in the same
      // document.
      if (previous_document &&
          previous_document == target_element->node_document()) {
        // The nearest ancestor of the target element that is already
        // designated is the nearest common ancestor of it and the previous
        // element.
        nearest_common_ancestor = target_element;
        while (nearest_common_ancestor &&
               nearest_common_ancestor->AsHTMLElement() &&
               !nearest_common_ancestor->AsHTMLElement()->IsDesignated()) {
          nearest_common_ancestor = nearest_common_ancestor->parent_element();
        }
      }
    }
  }
  return nearest_common_ancestor;
}

void SendStateChangeLeaveEvents(
    bool is_pointer_event, scoped_refptr<dom::HTMLElement> previous_element,
    scoped_refptr<dom::HTMLElement> target_element,
    scoped_refptr<dom::Element> nearest_common_ancestor,
    dom::PointerEventInit* event_init) {
  // Send enter/leave/over/out (status change) events when needed.
  if (previous_element != target_element) {
    const scoped_refptr<dom::Window>& view = event_init->view();

    // Send out and leave events.
    if (previous_element) {
      // LottiePlayer elements may change playback state.
      if (previous_element->AsLottiePlayer()) {
        previous_element->AsLottiePlayer()->OnUnHover();
      }

      dom::Document* previous_document = previous_element->node_document();

      event_init->set_related_target(target_element);
      if (is_pointer_event) {
        previous_element->DispatchEvent(new dom::PointerEvent(
            base::Tokens::pointerout(), view, *event_init));
        if (previous_document) {
          for (scoped_refptr<dom::Element> element = previous_element;
               element && element != nearest_common_ancestor;
               element = element->parent_element()) {
            DCHECK(element->AsHTMLElement()->IsDesignated());
            element->DispatchEvent(new dom::PointerEvent(
                base::Tokens::pointerleave(), web::Event::kNotBubbles,
                web::Event::kNotCancelable, view, *event_init));
          }
        }
      }

      // Send compatibility mapping mouse events for state changes.
      //   https://www.w3.org/TR/pointerevents/#mapping-for-devices-that-do-not-support-hover
      previous_element->DispatchEvent(
          new dom::MouseEvent(base::Tokens::mouseout(), view, *event_init));

      if (previous_document) {
        for (scoped_refptr<dom::Element> element = previous_element;
             element && element != nearest_common_ancestor;
             element = element->parent_element()) {
          DCHECK(element->AsHTMLElement()->IsDesignated());
          element->DispatchEvent(new dom::MouseEvent(
              base::Tokens::mouseleave(), web::Event::kNotBubbles,
              web::Event::kNotCancelable, view, *event_init));
        }

        if (!target_element ||
            previous_document != target_element->node_document()) {
          previous_document->SetIndicatedElement(NULL);
        }
      }
    }
  }
}

void SendStateChangeLeaveEvents(
    bool is_pointer_event, scoped_refptr<dom::HTMLElement> previous_element,
    dom::PointerEventInit* event_init) {
  scoped_refptr<dom::HTMLElement> target_element = nullptr;
  scoped_refptr<dom::Element> nearest_common_ancestor = nullptr;
  SendStateChangeLeaveEvents(is_pointer_event, previous_element, target_element,
                             nearest_common_ancestor, event_init);
}

void SendStateChangeEnterEvents(
    bool is_pointer_event, scoped_refptr<dom::HTMLElement> previous_element,
    scoped_refptr<dom::HTMLElement> target_element,
    scoped_refptr<dom::Element> nearest_common_ancestor,
    dom::PointerEventInit* event_init) {
  // Send enter/leave/over/out (status change) events when needed.
  if (previous_element != target_element) {
    const scoped_refptr<dom::Window>& view = event_init->view();

    // Send over and enter events.
    if (target_element) {
      // LottiePlayer elements may change playback state.
      if (target_element->AsLottiePlayer()) {
        target_element->AsLottiePlayer()->OnHover();
      }

      event_init->set_related_target(previous_element);
      if (is_pointer_event) {
        target_element->DispatchEvent(new dom::PointerEvent(
            base::Tokens::pointerover(), view, *event_init));
        for (scoped_refptr<dom::Element> element = target_element;
             element && element != nearest_common_ancestor;
             element = element->parent_element()) {
          element->DispatchEvent(new dom::PointerEvent(
              base::Tokens::pointerenter(), web::Event::kNotBubbles,
              web::Event::kNotCancelable, view, *event_init));
        }
      }

      // Send compatibility mapping mouse events for state changes.
      //   https://www.w3.org/TR/pointerevents/#mapping-for-devices-that-do-not-support-hover
      target_element->DispatchEvent(
          new dom::MouseEvent(base::Tokens::mouseover(), view, *event_init));
      for (scoped_refptr<dom::Element> element = target_element;
           element && element != nearest_common_ancestor;
           element = element->parent_element()) {
        element->DispatchEvent(new dom::MouseEvent(
            base::Tokens::mouseenter(), web::Event::kNotBubbles,
            web::Event::kNotCancelable, view, *event_init));
      }
    }
  }
}

void SendCompatibilityMappingMouseEvent(
    const scoped_refptr<dom::HTMLElement>& target_element,
    const scoped_refptr<web::Event>& event,
    const dom::PointerEvent* pointer_event,
    const dom::PointerEventInit& event_init,
    std::set<std::string>* mouse_event_prevent_flags) {
  // Send compatibility mapping mouse event if needed.
  //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#compatibility-mapping-with-mouse-events
  bool has_compatibility_mouse_event = true;
  base_token::Token type = pointer_event->type();
  if (type == base::Tokens::pointerdown()) {
    // If the pointer event dispatched was pointerdown and the event was
    // canceled, then set the PREVENT MOUSE EVENT flag for this pointerType.
    if (event->default_prevented()) {
      mouse_event_prevent_flags->insert(pointer_event->pointer_type());
      has_compatibility_mouse_event = false;
    } else {
      type = base::Tokens::mousedown();
    }
  } else {
    has_compatibility_mouse_event =
        mouse_event_prevent_flags->find(pointer_event->pointer_type()) ==
        mouse_event_prevent_flags->end();
    if (type == base::Tokens::pointerup()) {
      // If the pointer event dispatched was pointerup, clear the PREVENT
      // MOUSE EVENT flag for this pointerType.
      mouse_event_prevent_flags->erase(pointer_event->pointer_type());
      type = base::Tokens::mouseup();
    } else if (type == base::Tokens::pointermove()) {
      type = base::Tokens::mousemove();
    } else {
      has_compatibility_mouse_event = false;
    }
  }
  if (has_compatibility_mouse_event) {
    target_element->DispatchEvent(
        new dom::MouseEvent(type, event_init.view(), event_init));
  }
}

void InitializePointerEventInitFromEvent(
    const dom::MouseEvent* const mouse_event,
    const dom::PointerEvent* pointer_event, dom::PointerEventInit* event_init) {
  // For EventInit
  event_init->set_bubbles(mouse_event->bubbles());
  event_init->set_cancelable(mouse_event->cancelable());

  // For UIEventInit
  event_init->set_view(mouse_event->view());
  event_init->set_detail(mouse_event->detail());
  event_init->set_which(mouse_event->which());

  // For EventModifierInit
  event_init->set_ctrl_key(mouse_event->ctrl_key());
  event_init->set_shift_key(mouse_event->shift_key());
  event_init->set_alt_key(mouse_event->alt_key());
  event_init->set_meta_key(mouse_event->meta_key());

  // For MouseEventInit
  event_init->set_screen_x(mouse_event->screen_x());
  event_init->set_screen_y(mouse_event->screen_y());
  event_init->set_client_x(mouse_event->screen_x());
  event_init->set_client_y(mouse_event->screen_y());
  event_init->set_button(mouse_event->button());
  event_init->set_buttons(mouse_event->buttons());
  event_init->set_related_target(mouse_event->related_target());
  if (pointer_event) {
    // For PointerEventInit
    event_init->set_pointer_id(pointer_event->pointer_id());
    event_init->set_width(pointer_event->width());
    event_init->set_height(pointer_event->height());
    event_init->set_pressure(pointer_event->pressure());
    event_init->set_tilt_x(pointer_event->tilt_x());
    event_init->set_tilt_y(pointer_event->tilt_y());
    event_init->set_pointer_type(pointer_event->pointer_type());
    event_init->set_is_primary(pointer_event->is_primary());
  }
}

void DispatchPointerEventsForScrollStart(
    scoped_refptr<dom::HTMLElement> element,
    dom::PointerEventInit* event_init) {
  const scoped_refptr<dom::Window>& view = event_init->view();
  element->DispatchEvent(
      new dom::PointerEvent(base::Tokens::pointercancel(), web::Event::kBubbles,
                            web::Event::kNotCancelable, view, *event_init));
  bool is_pointer_event = true;
  SendStateChangeLeaveEvents(is_pointer_event, element, event_init);
}

math::Matrix3F GetCompleteTransformMatrix(dom::Element* element) {
  auto complete_matrix = math::Matrix3F::Identity();
  auto current_element = element;
  while (current_element) {
    LayoutBoxes* layout_boxes = GetLayoutBoxesIfNotEmpty(current_element);
    const Box* box = layout_boxes->boxes().front();
    complete_matrix = complete_matrix * box->GetCSSTransformForBox();
    current_element = current_element->parent_element();
  }
  return complete_matrix;
}

bool LayoutBoxesAreFixed(LayoutBoxes* layout_boxes) {
  const Boxes boxes = layout_boxes->boxes();
  const Box* box = boxes.front();
  if (!box->computed_style()) {
    return false;
  }

  return box->computed_style()->position() == cssom::KeywordValue::GetFixed();
}

bool ShouldConsiderElementAndChildren(dom::Element* element,
                                      math::Vector2dF* coordinate,
                                      bool consider_only_fixed_elements) {
  LayoutBoxes* layout_boxes = GetLayoutBoxesIfNotEmpty(element);
  if (!layout_boxes) {
    return false;
  }

  bool is_fixed_element = LayoutBoxesAreFixed(layout_boxes);
  if (consider_only_fixed_elements && !is_fixed_element) {
    return false;
  }
  return CanTargetElementAndChildren(element, coordinate);
}

}  // namespace

void TopmostEventTarget::ConsiderElement(dom::Element* element,
                                         const math::Vector2dF& coordinate,
                                         bool consider_only_fixed_elements) {
  if (!element) return;
  math::Vector2dF element_coordinate(coordinate);
  LayoutBoxes* layout_boxes = GetLayoutBoxesIfNotEmpty(element);
  bool consider_element_and_children = ShouldConsiderElementAndChildren(
      element, &element_coordinate, consider_only_fixed_elements);

  if (consider_element_and_children) {
    scoped_refptr<dom::HTMLElement> html_element = element->AsHTMLElement();
    if (html_element && html_element->CanBeDesignatedByPointerIfDisplayed()) {
      ConsiderBoxes(html_element, layout_boxes, element_coordinate);
    }
  }

  for (dom::Element* child_element = element->first_element_child();
       child_element; child_element = child_element->next_element_sibling()) {
    ConsiderElement(child_element, element_coordinate,
                    !consider_element_and_children);
  }
}

void TopmostEventTarget::ConsiderBoxes(
    const scoped_refptr<dom::HTMLElement>& html_element,
    LayoutBoxes* layout_boxes, const math::Vector2dF& coordinate) {
  const Boxes& boxes = layout_boxes->boxes();
  Vector2dLayoutUnit layout_coordinate(LayoutUnit(coordinate.x()),
                                       LayoutUnit(coordinate.y()));
  for (Boxes::const_iterator box_iterator = boxes.begin();
       box_iterator != boxes.end(); ++box_iterator) {
    Box* box = box_iterator->get();
    do {
      if (box->IsUnderCoordinate(layout_coordinate)) {
        Box::RenderSequence render_sequence = box->GetRenderSequence();
        if (Box::IsRenderedLater(render_sequence, render_sequence_)) {
          html_element_ = html_element;
          box_ = box;
          render_sequence_.swap(render_sequence);
        }
      }
      box = box->GetSplitSibling();
    } while (box != NULL);
  }
}

void TopmostEventTarget::CancelScrollsInParentNavItems(
    scoped_refptr<dom::HTMLElement> target_element) {
  // Cancel any scrolls in the tree.
  std::vector<scoped_refptr<ui_navigation::NavItem>> scrolls_to_cancel;
  auto current_element = target_element;
  while (true) {
    if (!current_element->parent_element()) {
      break;
    }
    current_element = current_element->parent_element()->AsHTMLElement();
    auto current_ui_nav_item = current_element->GetUiNavItem();
    if (current_ui_nav_item) {
      scrolls_to_cancel.push_back(current_ui_nav_item);
    }
  }

  scroll_engine_->thread()->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ui_navigation::scroll_engine::ScrollEngine::
                     CancelActiveScrollsForNavItems,
                 base::Unretained(scroll_engine_), scrolls_to_cancel));
}

void TopmostEventTarget::HandleScrollState(
    scoped_refptr<dom::HTMLElement> target_element,
    const dom::PointerEvent* pointer_event, dom::PointerState* pointer_state,
    dom::PointerEventInit* event_init) {
  // On pointer down, cancel any scrolls happening for UI nav items above
  // that element. Additionally, save the pointer coordinates.
  //
  // On pointer move, check if we've reached the threshold to start
  // scrolling. If we have, find the first scroll container we can scroll.
  // Then send that scroll container, initial pointer event coords, current
  // pointer event coords, scroll direction.
  bool pointer_type_is_accepted = pointer_event->pointer_type() == "mouse" ||
                                  pointer_event->pointer_type() == "pen" ||
                                  pointer_event->pointer_type() == "touch";
  if (!scroll_engine_ || !pointer_event || !pointer_type_is_accepted) {
    return;
  }

  bool should_clear_pointer_state =
      pointer_event->type() == base::Tokens::pointerup();

  auto pointer_id = pointer_event->pointer_id();
  auto pointer_coordinates =
      math::Vector2dF(pointer_event->client_x(), pointer_event->client_y());

  if (pointer_event->type() == base::Tokens::pointerdown()) {
    CancelScrollsInParentNavItems(target_element);

    auto initial_possible_scroll_targets =
        FindPossibleScrollTargets(target_element);
    pointer_state->SetPossibleScrollTargets(
        pointer_id, std::move(initial_possible_scroll_targets));

    auto transform_matrix = GetCompleteTransformMatrix(target_element.get());
    pointer_state->SetClientTransformMatrix(pointer_id, transform_matrix);
    pointer_state->SetClientCoordinates(pointer_id, pointer_coordinates);
    pointer_state->SetClientTimeStamp(pointer_id, pointer_event->time_stamp());
    return;
  }

  auto initial_coordinates = pointer_state->GetClientCoordinates(pointer_id);
  auto initial_time_stamp = pointer_state->GetClientTimeStamp(pointer_id);
  auto possible_scroll_targets =
      pointer_state->GetPossibleScrollTargets(pointer_id);
  auto initial_transform = pointer_state->GetClientTransformMatrix(pointer_id);

  if (pointer_event->type() == base::Tokens::pointermove() &&
      initial_coordinates.has_value() && initial_time_stamp.has_value() &&
      possible_scroll_targets) {
    cobalt::math::Vector2dF drag_vector =
        initial_coordinates.value() - pointer_coordinates;
    float x = drag_vector.x();
    float y = drag_vector.y();

    if (drag_vector.Length() >=
        ui_navigation::scroll_engine::kDragDistanceThreshold) {
      // Get major scroll direction.
      ui_navigation::scroll_engine::ScrollType scroll_type =
          std::abs(x) > std::abs(y)
              ? ui_navigation::scroll_engine::ScrollType::Horizontal
              : ui_navigation::scroll_engine::ScrollType::Vertical;
      auto element_to_scroll = FindFirstElementWithScrollType(
          possible_scroll_targets, scroll_type, x > 0, y > 0);
      if (!element_to_scroll) {
        return;
      }

      DispatchPointerEventsForScrollStart(target_element, event_init);
      pointer_state->SetWasCancelled(pointer_id);

      should_clear_pointer_state = true;
      scroll_engine_->thread()->message_loop()->task_runner()->PostTask(
          FROM_HERE,
          base::Bind(
              &ui_navigation::scroll_engine::ScrollEngine::HandleScrollStart,
              base::Unretained(scroll_engine_),
              element_to_scroll->GetUiNavItem(), scroll_type, pointer_id,
              initial_coordinates.value(), initial_time_stamp.value(),
              pointer_coordinates, pointer_event->time_stamp(),
              initial_transform));
    }
  }

  if (should_clear_pointer_state) {
    pointer_state->ClearPossibleScrollTargets(pointer_id);
    pointer_state->ClearClientCoordinates(pointer_id);
    pointer_state->ClearTimeStamp(pointer_id);
    pointer_state->ClearMatrix(pointer_id);
  }
}

TopmostEventTarget::TopmostEventTarget(
    ui_navigation::scroll_engine::ScrollEngine* scroll_engine)
    : scroll_engine_(scroll_engine) {}

void TopmostEventTarget::MaybeSendPointerEvents(
    const scoped_refptr<web::Event>& event) {
  TRACE_EVENT0("cobalt::layout",
               "TopmostEventTarget::MaybeSendPointerEvents()");

  const dom::MouseEvent* const mouse_event =
      base::polymorphic_downcast<const dom::MouseEvent* const>(event.get());
  DCHECK(mouse_event);
  DCHECK(!html_element_);
  const dom::PointerEvent* pointer_event =
      (event->GetWrappableType() == base::GetTypeId<dom::PointerEvent>())
          ? base::polymorphic_downcast<const dom::PointerEvent* const>(
                event.get())
          : NULL;
  bool is_touchpad_event = false;

  // The target override element for the pointer event. This may not be the same
  // as the hit test target, and it also may not be set.
  scoped_refptr<dom::HTMLElement> target_override_element;

  // Store the data for the status change and pointer capture event(s).
  dom::PointerEventInit event_init;
  InitializePointerEventInitFromEvent(mouse_event, pointer_event, &event_init);
  const scoped_refptr<dom::Window>& view = event_init.view();
  if (!view) {
    return;
  }
  dom::PointerState* pointer_state = view->document()->pointer_state();
  if (pointer_event) {
    pointer_state->SetActiveButtonsState(pointer_event->pointer_id(),
                                         pointer_event->buttons());
    is_touchpad_event = pointer_event->pointer_type() == "touchpad";
    if (is_touchpad_event) {
      if (pointer_event->type() == base::Tokens::pointerdown()) {
        pointer_state->SetActive(pointer_event->pointer_id());
        // Implicitly capture the pointer to the active element.
        //   https://www.w3.org/TR/pointerevents/#implicit-pointer-capture
        scoped_refptr<dom::HTMLElement> html_element;
        if (view->document()->active_element()) {
          html_element = view->document()->active_element()->AsHTMLElement();
        }
        if (html_element) {
          pointer_state->SetPendingPointerCaptureTargetOverride(
              pointer_event->pointer_id(), html_element);
        }
      }
    } else {
      pointer_state->SetActive(pointer_event->pointer_id());
    }
    target_override_element = pointer_state->GetPointerCaptureOverrideElement(
        pointer_event->pointer_id(), &event_init);
  }

  scoped_refptr<dom::HTMLElement> target_element;
  if (target_override_element) {
    target_element = target_override_element;
  } else {
    // Do a hit test if there is no target override element.
    math::Vector2dF coordinate(static_cast<float>(event_init.client_x()),
                               static_cast<float>(event_init.client_y()));
    target_element = FindTopmostEventTarget(view->document(), coordinate);
  }

  if (target_element && pointer_event) {
    HandleScrollState(target_element, pointer_event, pointer_state,
                      &event_init);
  }

  scoped_refptr<dom::HTMLElement> previous_html_element(
      previous_html_element_weak_);

  // The enter/leave status change events apply to all ancestors up to the
  // nearest common ancestor between the previous and current element.
  scoped_refptr<dom::Element> nearest_common_ancestor(
      GetNearestCommonAncestor(previous_html_element, target_element));

  SendStateChangeLeaveEvents(pointer_event, previous_html_element,
                             target_element, nearest_common_ancestor,
                             &event_init);

  bool event_was_cancelled = pointer_event && pointer_state->GetWasCancelled(
                                                  pointer_event->pointer_id());
  if (pointer_event && pointer_event->type() == base::Tokens::pointerup()) {
    pointer_state->ClearWasCancelled(pointer_event->pointer_id());
  }

  if (target_element) {
    if (!event_was_cancelled) {
      target_element->DispatchEvent(event);
    }
  }

  if (pointer_event) {
    if (pointer_event->type() == base::Tokens::pointerup()) {
      if (is_touchpad_event) {
        // A touchpad becomes inactive after a pointerup.
        pointer_state->ClearActive(pointer_event->pointer_id());
      }
      // Implicit release of pointer capture.
      //   https://www.w3.org/TR/pointerevents/#implicit-release-of-pointer-capture
      pointer_state->ClearPendingPointerCaptureTargetOverride(
          pointer_event->pointer_id());
    }
    if (target_element && !is_touchpad_event && !event_was_cancelled) {
      SendCompatibilityMappingMouseEvent(target_element, event, pointer_event,
                                         event_init,
                                         &mouse_event_prevent_flags_);
    }
  }

  if (event_init.button() == 0 &&
      ((mouse_event->type() == base::Tokens::pointerup()) ||
       (mouse_event->type() == base::Tokens::mouseup()))) {
    // This is an 'up' event for the last pressed button indicating that no
    // more buttons are pressed.
    if (target_element && !is_touchpad_event) {
      // Send the click event if needed, which is not prevented by cancelling
      // the pointerdown event.
      //   https://www.w3.org/TR/uievents/#event-type-click
      //   https://www.w3.org/TR/pointerevents/#compatibility-mapping-with-mouse-events
      target_element->DispatchEvent(
          new dom::MouseEvent(base::Tokens::click(), view, event_init));
    }
    if (target_element && (pointer_event->pointer_type() != "mouse")) {
      // If it's not a mouse event, then releasing the last button means
      // that there is no longer an indicated element.
      dom::Document* document = target_element->node_document();
      if (document) {
        document->SetIndicatedElement(NULL);
        target_element = NULL;
      }
    }
  }

  SendStateChangeEnterEvents(pointer_event, previous_html_element,
                             target_element, nearest_common_ancestor,
                             &event_init);

  if (target_element) {
    // Touchpad input never indicates document elements.
    if (!is_touchpad_event) {
      dom::Document* document = target_element->node_document();
      if (document) {
        document->SetIndicatedElement(target_element);
      }
    }
    previous_html_element_weak_ = base::AsWeakPtr(target_element.get());
  } else {
    previous_html_element_weak_.reset();
  }
  DCHECK(!html_element_);
}

}  // namespace layout
}  // namespace cobalt
