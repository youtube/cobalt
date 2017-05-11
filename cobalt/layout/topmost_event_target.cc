// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/mouse_event.h"
#include "cobalt/dom/mouse_event_init.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/dom/pointer_event_init.h"
#include "cobalt/dom/ui_event.h"
#include "cobalt/dom/wheel_event.h"
#include "cobalt/layout/container_box.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/math/vector2d.h"
#include "cobalt/math/vector2d_f.h"

namespace cobalt {
namespace layout {

void TopmostEventTarget::FindTopmostEventTarget(
    const scoped_refptr<dom::Document>& document,
    const math::Vector2dF& coordinate) {
  const scoped_refptr<dom::HTMLElement>& html_element = document->html();
  box_ = NULL;
  html_element_ = html_element;
  render_sequence_.clear();
  if (html_element) {
    dom::LayoutBoxes* boxes = html_element->layout_boxes();
    if (boxes && boxes->type() == dom::LayoutBoxes::kLayoutLayoutBoxes) {
      LayoutBoxes* layout_boxes = base::polymorphic_downcast<LayoutBoxes*>(
          html_element->layout_boxes());
      if (!layout_boxes->boxes().empty()) {
        ConsiderElement(html_element, coordinate);
      }
    }
  }
}

void TopmostEventTarget::ConsiderElement(
    const scoped_refptr<dom::HTMLElement>& html_element,
    const math::Vector2dF& coordinate) {
  if (!html_element) return;
  math::Vector2dF element_coordinate(coordinate);
  dom::LayoutBoxes* boxes = html_element->layout_boxes();
  if (boxes && boxes->type() == dom::LayoutBoxes::kLayoutLayoutBoxes) {
    SB_DCHECK(html_element->computed_style());
    LayoutBoxes* layout_boxes = base::polymorphic_downcast<LayoutBoxes*>(boxes);
    const Boxes& boxes = layout_boxes->boxes();
    if (!boxes.empty()) {
      const Box* box = boxes.front();
      box->UpdateCoordinateForTransform(&element_coordinate);

      if (box->computed_style()->position() ==
          cssom::KeywordValue::GetAbsolute()) {
        // The containing block for position:absolute elements is formed by the
        // padding box instead of the content box, as described in
        // http://www.w3.org/TR/CSS21/visudet.html#containing-block-details.
        element_coordinate +=
            box->GetContainingBlock()->GetContentBoxOffsetFromPaddingBox();
      }
      ConsiderBoxes(html_element, layout_boxes, element_coordinate);
    }
  }

  for (dom::Element* element = html_element->first_element_child(); element;
       element = element->next_element_sibling()) {
    dom::HTMLElement* child_html_element = element->AsHTMLElement();
    if (child_html_element && child_html_element->computed_style()) {
      ConsiderElement(child_html_element, element_coordinate);
    }
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
    const scoped_refptr<Box>& box = *box_iterator;
    if (box->IsUnderCoordinate(layout_coordinate)) {
      Box::RenderSequence render_sequence = box->GetRenderSequence();
      if (Box::IsRenderedLater(render_sequence, render_sequence_)) {
        html_element_ = html_element;
        box_ = box;
        render_sequence_.swap(render_sequence);
      }
    }
  }
}

void TopmostEventTarget::MaybeSendPointerEvents(
    const scoped_refptr<dom::Event>& event,
    const scoped_refptr<dom::Window>& window) {
  const dom::MouseEvent* const mouse_event =
      base::polymorphic_downcast<const dom::MouseEvent* const>(event.get());
  SB_DCHECK(mouse_event);
  const scoped_refptr<dom::Document>& document =
      mouse_event->view()->document();

  math::Vector2dF coordinate(mouse_event->client_x(), mouse_event->client_y());
  FindTopmostEventTarget(document, coordinate);

  if (html_element_) {
    html_element_->DispatchEvent(event);
  }
  if (event->GetWrappableType() == base::GetTypeId<dom::PointerEvent>()) {
    const dom::PointerEvent* const pointer_event =
        base::polymorphic_downcast<const dom::PointerEvent* const>(event.get());

    // Send compatibility mapping mouse events if needed.
    //  https://www.w3.org/TR/2015/REC-pointerevents-20150224/#compatibility-mapping-with-mouse-events
    if (html_element_) {
      bool has_compatibility_mouse_event = false;
      base::Token type;
      if (pointer_event->type() == base::Tokens::pointerdown()) {
        type = base::Tokens::mousedown();
        has_compatibility_mouse_event = true;
      } else if (pointer_event->type() == base::Tokens::pointerup()) {
        type = base::Tokens::mouseup();
        has_compatibility_mouse_event = true;
      } else if (pointer_event->type() == base::Tokens::pointermove()) {
        type = base::Tokens::mousemove();
        has_compatibility_mouse_event = true;
      }
      if (has_compatibility_mouse_event) {
        dom::MouseEventInit mouse_event_init;
        mouse_event_init.set_screen_x(pointer_event->screen_x());
        mouse_event_init.set_screen_y(pointer_event->screen_x());
        mouse_event_init.set_client_x(pointer_event->screen_x());
        mouse_event_init.set_client_y(pointer_event->screen_x());
        mouse_event_init.set_button(pointer_event->button());
        mouse_event_init.set_buttons(pointer_event->buttons());
        html_element_->DispatchEvent(
            new dom::MouseEvent(type, window, mouse_event_init));
        if (pointer_event->type() == base::Tokens::pointerup()) {
          type = base::Tokens::click();
          html_element_->DispatchEvent(
              new dom::MouseEvent(type, window, mouse_event_init));
        }
      }
    }

    // Send enter/leave/over/out (status change) events when needed.
    if (previous_html_element_ != html_element_) {
      // Store the data for the status change event(s).
      dom::PointerEventInit event_init;
      event_init.set_related_target(previous_html_element_);
      const dom::MouseEvent* const pointer_event =
          base::polymorphic_downcast<const dom::PointerEvent* const>(
              event.get());
      event_init.set_screen_x(pointer_event->screen_x());
      event_init.set_screen_y(pointer_event->screen_x());
      event_init.set_client_x(pointer_event->screen_x());
      event_init.set_client_y(pointer_event->screen_x());
      if (event->GetWrappableType() == base::GetTypeId<dom::PointerEvent>()) {
        const dom::PointerEvent* const pointer_event =
            base::polymorphic_downcast<const dom::PointerEvent* const>(
                event.get());
        event_init.set_pointer_id(pointer_event->pointer_id());
        event_init.set_width(pointer_event->width());
        event_init.set_height(pointer_event->height());
        event_init.set_pressure(pointer_event->pressure());
        event_init.set_tilt_x(pointer_event->tilt_x());
        event_init.set_tilt_y(pointer_event->tilt_y());
        event_init.set_pointer_type(pointer_event->pointer_type());
        event_init.set_is_primary(pointer_event->is_primary());
      }

      // The enter/leave status change events apply to all ancestors up to the
      // nearest common ancestor between the previous and current element.
      scoped_refptr<dom::Element> nearest_common_ancestor;

      if (previous_html_element_) {
        previous_html_element_->DispatchEvent(new dom::PointerEvent(
            base::Tokens::pointerout(), window, event_init));
        previous_html_element_->DispatchEvent(
            new dom::MouseEvent(base::Tokens::mouseout(), window, event_init));

        // Find the nearest common ancestor, if there is any.
        dom::Document* previous_document =
            previous_html_element_->node_document();
        if (previous_document) {
          if (html_element_ &&
              previous_document == html_element_->node_document()) {
            // The nearest ancestor of the current element that is already
            // designated is the nearest common ancestor of it and the previous
            // element.
            nearest_common_ancestor = html_element_;
            while (nearest_common_ancestor &&
                   nearest_common_ancestor->AsHTMLElement() &&
                   !nearest_common_ancestor->AsHTMLElement()->IsDesignated()) {
              nearest_common_ancestor =
                  nearest_common_ancestor->parent_element();
            }
          }

          for (scoped_refptr<dom::Element> element = previous_html_element_;
               element != nearest_common_ancestor;
               element = element->parent_element()) {
            element->DispatchEvent(new dom::PointerEvent(
                base::Tokens::pointerleave(), dom::Event::kNotBubbles,
                dom::Event::kNotCancelable, window, event_init));
            element->DispatchEvent(new dom::MouseEvent(
                base::Tokens::mouseleave(), dom::Event::kNotBubbles,
                dom::Event::kNotCancelable, window, event_init));
          }

          if (!html_element_ ||
              previous_document != html_element_->node_document()) {
            previous_document->SetIndicatedElement(NULL);
          }
        }
      }
      if (html_element_) {
        html_element_->DispatchEvent(new dom::PointerEvent(
            base::Tokens::pointerover(), window, event_init));
        html_element_->DispatchEvent(
            new dom::MouseEvent(base::Tokens::mouseover(), window, event_init));

        for (scoped_refptr<dom::Element> element = html_element_;
             element != nearest_common_ancestor;
             element = element->parent_element()) {
          element->DispatchEvent(new dom::PointerEvent(
              base::Tokens::pointerenter(), dom::Event::kNotBubbles,
              dom::Event::kNotCancelable, window, event_init));
          element->DispatchEvent(new dom::MouseEvent(
              base::Tokens::mouseenter(), dom::Event::kNotBubbles,
              dom::Event::kNotCancelable, window, event_init));
        }

        dom::Document* document = html_element_->node_document();
        if (document) {
          document->SetIndicatedElement(html_element_);
        }
      }
      previous_html_element_ = html_element_;
    }
  }
}

}  // namespace layout
}  // namespace cobalt
