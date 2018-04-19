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

#include "cobalt/dom/pointer_state.h"

#include <algorithm>

#include "cobalt/dom/mouse_event.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/dom/wheel_event.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

void PointerState::QueuePointerEvent(const scoped_refptr<Event>& event) {
  // Only accept this for event types that are MouseEvents or known derivatives.
  SB_DCHECK(event->GetWrappableType() == base::GetTypeId<PointerEvent>() ||
            event->GetWrappableType() == base::GetTypeId<MouseEvent>() ||
            event->GetWrappableType() == base::GetTypeId<WheelEvent>());

  // Queue the event to be handled on the next layout.
  pointer_events_.push(event);
}

namespace {
int32_t GetPointerIdFromEvent(const scoped_refptr<Event>& event) {
  if (event->GetWrappableType() == base::GetTypeId<PointerEvent>()) {
    const PointerEvent* const pointer_event =
        base::polymorphic_downcast<const PointerEvent* const>(event.get());
    return pointer_event->pointer_id();
  }
  return 0;
}
}  // namespace

scoped_refptr<Event> PointerState::GetNextQueuedPointerEvent() {
  scoped_refptr<Event> event;
  if (pointer_events_.empty()) {
    return event;
  }

  // Ignore pointer move events when they are succeeded by additional pointer
  // move events with the same pointerId.
  scoped_refptr<Event> front_event(pointer_events_.front());
  bool next_event_is_move_event =
      front_event->type() == base::Tokens::pointermove() ||
      front_event->type() == base::Tokens::mousemove();
  int32_t next_event_pointer_id = GetPointerIdFromEvent(front_event);
  int32_t current_event_pointer_id;
  bool current_event_is_move_event;
  do {
    current_event_pointer_id = next_event_pointer_id;
    current_event_is_move_event = next_event_is_move_event;
    event = pointer_events_.front();
    pointer_events_.pop();
    if (pointer_events_.empty() || !current_event_is_move_event) {
      break;
    }
    next_event_pointer_id = GetPointerIdFromEvent(event);
    if (next_event_pointer_id != current_event_pointer_id) {
      break;
    }
    front_event = pointer_events_.front();
    next_event_is_move_event =
        (front_event->type() == base::Tokens::pointermove() ||
         front_event->type() == base::Tokens::mousemove());
  } while (next_event_is_move_event);
  return event;
}

void PointerState::SetPendingPointerCaptureTargetOverride(int32_t pointer_id,
                                                          Element* element) {
  pending_target_override_[pointer_id] = base::AsWeakPtr(element);
}

void PointerState::ClearPendingPointerCaptureTargetOverride(
    int32_t pointer_id) {
  pending_target_override_.erase(pointer_id);
}

scoped_refptr<HTMLElement> PointerState::GetPointerCaptureOverrideElement(
    int32_t pointer_id, PointerEventInit* event_init) {
  const scoped_refptr<Window>& view = event_init->view();
  scoped_refptr<Element> target_override_element;
  // Algorithm for Process Pending Pointer Capture.
  //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#process-pending-pointer-capture
  auto pending_override = pending_target_override_.find(pointer_id);
  auto override = target_override_.find(pointer_id);

  if (pending_override != pending_target_override_.end() &&
      pending_override->second &&
      pending_override->second->node_document() != view->document()) {
    // When the pointer capture target override is removed from its
    // ownerDocument's tree, clear the pending pointer capture target override
    // and pointer capture target override nodes and fire a PointerEvent named
    // lostpointercapture at the document.
    pending_target_override_.erase(pending_override);
    pending_override = pending_target_override_.end();
  }

  // 1. If the pointer capture target override for this pointer is set and is
  // not equal to the pending pointer capture target override, then fire a
  // pointer event named lostpointercapture at the pointer capture target
  // override node.
  if (override != target_override_.end() && override->second &&
      (pending_override == pending_target_override_.end() ||
       pending_override->second != override->second)) {
    override->second->DispatchEvent(
        new PointerEvent(base::Tokens::lostpointercapture(), Event::kBubbles,
                         Event::kNotCancelable, view, *event_init));
  }

  // 2. If the pending pointer capture target override for this pointer is set
  // and is not equal to the pointer capture target override, then fire a
  // pointer event named gotpointercapture at the pending pointer capture
  // target override.
  if (pending_override != pending_target_override_.end() &&
      pending_override->second &&
      (override == target_override_.end() ||
       pending_override->second != override->second)) {
    pending_override->second->DispatchEvent(
        new PointerEvent(base::Tokens::gotpointercapture(), Event::kBubbles,
                         Event::kNotCancelable, view, *event_init));
  }

  // 3. Set the pointer capture target override to the pending pointer capture
  // target override, if set. Otherwise, clear the pointer capture target
  // override.
  if (pending_override != pending_target_override_.end() &&
      pending_override->second) {
    target_override_[pending_override->first] = pending_override->second;
    target_override_element = pending_override->second;
  } else if (override != target_override_.end()) {
    target_override_.erase(override);
  }
  scoped_refptr<HTMLElement> html_element;
  if (target_override_element) {
    html_element = target_override_element->AsHTMLElement();
  }
  return html_element;
}

void PointerState::SetPointerCapture(int32_t pointer_id, Element* element,
                                     script::ExceptionState* exception_state) {
  UNREFERENCED_PARAMETER(element);
  // Algorithm for Setting Pointer Capture
  //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#setting-pointer-capture

  // 1. If the pointerId provided as the method's argument does not match any of
  // the active pointers, then throw a DOMException with the name
  // InvalidPointerId.
  if (active_pointers_.find(pointer_id) == active_pointers_.end()) {
    DOMException::Raise(dom::DOMException::kInvalidPointerIdErr,
                        exception_state);
    return;
  }

  // 2. If the Element on which this method is invoked does not participate in
  // its ownerDocument's tree, throw an exception with the name
  // InvalidStateError.
  if (!element || !element->owner_document()) {
    DOMException::Raise(dom::DOMException::kInvalidStateErr, exception_state);
    return;
  }

  // 3. If the pointer is not in the active buttons state, then terminate these
  // steps.
  if (pointers_with_active_buttons_.find(pointer_id) ==
      pointers_with_active_buttons_.end()) {
    return;
  }

  // 4. For the specified pointerId, set the pending pointer capture target
  // override to the Element on which this method was invoked.
  SetPendingPointerCaptureTargetOverride(pointer_id, element);
}

void PointerState::ReleasePointerCapture(
    int32_t pointer_id, Element* element,
    script::ExceptionState* exception_state) {
  UNREFERENCED_PARAMETER(element);
  // Algorithm for Releasing Pointer Capture
  //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#releasing-pointer-capture

  // 1. If the pointerId provided as the method's argument does not match any of
  // the active pointers and these steps are not being invoked as a result of
  // the implicit release of pointer capture, then throw a DOMException with the
  // name InvalidPointerId.
  if (active_pointers_.find(pointer_id) == active_pointers_.end()) {
    DOMException::Raise(dom::DOMException::kInvalidPointerIdErr,
                        exception_state);
    return;
  }

  // 2. If pointer capture is not currently set for the specified pointer, then
  // terminate these steps.
  auto pending_override = pending_target_override_.find(pointer_id);
  if (pending_override == pending_target_override_.end()) {
    return;
  }

  // 3. If the pointer capture target override for the specified pointerId is
  // not the Element on which this method was invoked, then terminate these
  // steps.
  if (pending_override->second.get() != element) {
    return;
  }

  // 4. For the specified pointerId, clear the pending pointer capture target
  // override, if set.
  ClearPendingPointerCaptureTargetOverride(pointer_id);
}

bool PointerState::HasPointerCapture(int32_t pointer_id, Element* element) {
  auto pending_override = pending_target_override_.find(pointer_id);

  if (pending_override != pending_target_override_.end()) {
    return pending_override->second.get() == element;
  }
  return false;
}

void PointerState::SetActiveButtonsState(int32_t pointer_id, uint16_t buttons) {
  if (buttons) {
    pointers_with_active_buttons_.insert(pointer_id);
  } else {
    pointers_with_active_buttons_.erase(pointer_id);
  }
}

void PointerState::SetActive(int32_t pointer_id) {
  active_pointers_.insert(pointer_id);
}

void PointerState::ClearActive(int32_t pointer_id) {
  active_pointers_.erase(pointer_id);
}

void PointerState::ClearForShutdown() {
  {
    decltype(pointer_events_) empty_queue;
    std::swap(pointer_events_, empty_queue);
  }
  target_override_.clear();
  pending_target_override_.clear();
  active_pointers_.clear();
  pointers_with_active_buttons_.clear();
}

}  // namespace dom
}  // namespace cobalt
