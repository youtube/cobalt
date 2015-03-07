/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DOM_EVENT_LISTENER_H_
#define DOM_EVENT_LISTENER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/dom/event.h"
#include "cobalt/script/script_object_handle_visitor.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The EventListener interface represents a callable object that will be called
// when an event is fired.
//   http://www.w3.org/TR/2014/WD-dom-20140710/#eventtarget
class EventListener : public script::Wrappable {
 public:
  // Web API: EventListener
  //
  virtual void HandleEvent(const scoped_refptr<Event>& event) = 0;

  // Custom, not in any spec.
  //
  // Marks necessary JS objects as in use so they won't be collected by GC.
  virtual void MarkJSObjectAsNotCollectable(
      script::ScriptObjectHandleVisitor* visitor) = 0;
  // Used by addEventListener/removeEventListener to check if two event
  // listeners are the same.
  virtual bool EqualTo(const EventListener& that) = 0;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_EVENT_LISTENER_H_
