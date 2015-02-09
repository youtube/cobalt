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

#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace dom {

void EventTarget::AddEventListener(const std::string& type,
                                   const scoped_refptr<EventListener>& listener,
                                   bool use_capture) {
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if (iter->type == type && iter->listener->EqualTo(*listener) &&
        iter->use_capture == use_capture) {
      return;
    }
  }

  EventListenerInfo info = {type, listener, use_capture};
  event_listener_infos_.push_back(info);
}

void EventTarget::RemoveEventListener(
    const std::string& type, const scoped_refptr<EventListener>& listener,
    bool use_capture) {
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if (iter->type == type && iter->listener->EqualTo(*listener) &&
        iter->use_capture == use_capture) {
      event_listener_infos_.erase(iter);
      return;
    }
  }
}

// TODO(***REMOVED***): Implement proper event propagation.
bool EventTarget::DispatchEvent(const scoped_refptr<Event>& event) {
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if (iter->type == event->type()) {
      iter->listener->HandleEvent(event);
    }
  }

  return true;
}

void EventTarget::MarkJSObjectAsNotCollectable(
    script::ScriptObjectHandleVisitor* visitor) {
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    iter->listener->MarkJSObjectAsNotCollectable(visitor);
  }
}

}  // namespace dom
}  // namespace cobalt
