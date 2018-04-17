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

#ifndef COBALT_DOM_POINTER_STATE_H_
#define COBALT_DOM_POINTER_STATE_H_

#include <map>
#include <queue>
#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/pointer_event_init.h"
#include "cobalt/math/vector2d_f.h"

namespace cobalt {
namespace dom {

// This class contains various state related to pointer and mouse support.
class PointerState {
 public:
  // Web API: Pointer Events: Extensions to the Element Interface (partial
  // interface)
  //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#extensions-to-the-element-interface
  void SetPointerCapture(int32_t pointer_id, Element* element,
                         script::ExceptionState* exception_state);
  void ReleasePointerCapture(int32_t pointer_id, Element* element,
                             script::ExceptionState* exception_state);
  bool HasPointerCapture(int32_t pointer_id, Element* element);

  // Custom: Not in any Web API.
  //

  // Queue up pointer related events.
  void QueuePointerEvent(const scoped_refptr<Event>& event);

  // Get the next queued pointer event.
  scoped_refptr<Event> GetNextQueuedPointerEvent();

  // Set the pending pointer capture target override for a pointer.
  void SetPendingPointerCaptureTargetOverride(int32_t pointer_id,
                                              Element* element);

  // Clear the pending pointer capture target override for a pointer.
  void ClearPendingPointerCaptureTargetOverride(int32_t pointer_id);

  // Process Pending Pointer Capture and return the capture override element.
  scoped_refptr<HTMLElement> GetPointerCaptureOverrideElement(
      int32_t pointer_id, PointerEventInit* event_init);

  // Set the 'active buttons state' for a pointer.
  //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#dfn-active-buttons-state
  void SetActiveButtonsState(int32_t pointer_id, uint16_t buttons);

  // Set of clear a pointer as 'active'.
  //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#dfn-active-pointer
  void SetActive(int32_t pointer_id);
  void ClearActive(int32_t pointer_id);

  // We will in general hold references back to window, which can result in
  // leaking nearly the entire DOM if we don't forcibly clear them during
  // shutdown.
  void ClearForShutdown();

 private:
  // Stores pointer events until they are handled after a layout.
  std::queue<scoped_refptr<Event> > pointer_events_;

  // This stores the elements with target overrides
  //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#pointer-capture
  std::map<int32_t, base::WeakPtr<Element> > target_override_;
  std::map<int32_t, base::WeakPtr<Element> > pending_target_override_;

  // Store the set of active pointers.
  //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#dfn-active-pointer
  std::set<int32_t> active_pointers_;

  // Store the set of pointers with active buttons.
  //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#dfn-active-buttons-state
  std::set<int32_t> pointers_with_active_buttons_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_POINTER_STATE_H_
