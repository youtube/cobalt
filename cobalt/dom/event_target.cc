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
  DCHECK(listener);
  DCHECK(!listener->IsAttribute());

  AddEventListenerInternal(type, listener, use_capture);
}

void EventTarget::RemoveEventListener(
    const std::string& type, const scoped_refptr<EventListener>& listener,
    bool use_capture) {
  DCHECK(listener);
  DCHECK(!listener->IsAttribute());

  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if (iter->type == type && iter->listener->EqualTo(*listener) &&
        iter->use_capture == use_capture) {
      event_listener_infos_.erase(iter);
      return;
    }
  }
}

// Dispatch event to a single event target outside the DOM tree. The event
// propagation in the DOM tree is implemented inside Node::DispatchEvent().
bool EventTarget::DispatchEvent(const scoped_refptr<Event>& event) {
  DCHECK(event);
  // TODO(***REMOVED***): Raise InvalidStateError exception.
  DCHECK(event->initialized_flag());

  event->set_target(this);
  event->set_event_phase(Event::kAtTarget);
  FireEventOnListeners(event);
  event->set_event_phase(Event::kNone);
  return !event->default_prevented();
}

void EventTarget::SetAttributeEventListener(
    const std::string& type, const scoped_refptr<EventListener>& listener) {
  // Remove existing attribute listener of the same type.
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if (iter->type == type && iter->listener->IsAttribute()) {
      event_listener_infos_.erase(iter);
      break;
    }
  }

  if (!listener) {
    return;
  }

  DCHECK(listener->IsAttribute());
  if (!listener->IsAttribute()) {
    return;
  }

  AddEventListenerInternal(type, listener, false);
}

scoped_refptr<EventListener> EventTarget::GetAttributeEventListener(
    const std::string& type) {
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if (iter->listener->IsAttribute() && iter->type == type) {
      return iter->listener;
    }
  }
  return NULL;
}

void EventTarget::FireEventOnListeners(const scoped_refptr<Event>& event) {
  DCHECK(event->IsBeingDispatched());
  DCHECK(event->target());
  DCHECK(!event->current_target());

  event->set_current_target(this);

  EventListenerInfos event_listener_infos(event_listener_infos_);

  for (EventListenerInfos::iterator iter = event_listener_infos.begin();
       iter != event_listener_infos.end(); ++iter) {
    if (event->immediate_propagation_stopped()) {
      continue;
    }
    if (iter->type != event->type()) {
      continue;
    }
    // Only call listeners marked as capture during capturing phase.
    if (event->event_phase() == Event::kCapturingPhase && !iter->use_capture) {
      continue;
    }
    // Don't call any listeners marked as capture during bubbling phase.
    if (event->event_phase() == Event::kBubblingPhase && iter->use_capture) {
      continue;
    }
    iter->listener->HandleEvent(event);
  }

  event->set_current_target(NULL);
}

void EventTarget::AddEventListenerInternal(
    const std::string& type, const scoped_refptr<EventListener>& listener,
    bool use_capture) {
  DCHECK(listener);

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

}  // namespace dom
}  // namespace cobalt
