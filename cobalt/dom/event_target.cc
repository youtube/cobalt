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

#include "cobalt/dom/dom_exception.h"

namespace cobalt {
namespace dom {

void EventTarget::AddEventListener(const std::string& type,
                                   const EventListenerScriptObject& listener,
                                   bool use_capture) {
  // Do nothing if listener is null.
  if (listener.IsNull()) {
    return;
  }

  EventListenerScriptObject::Reference listener_reference(this, listener);

  DCHECK(!listener_reference.value().IsAttribute());

  AddEventListenerInternal(type, listener, use_capture);
}

void EventTarget::RemoveEventListener(const std::string& type,
                                      const EventListenerScriptObject& listener,
                                      bool use_capture) {
  DCHECK(!listener.IsNull());
  EventListenerScriptObject::Reference listener_reference(this, listener);
  DCHECK(!listener_reference.value().IsAttribute());

  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->type == type &&
        (*iter)->listener.value().EqualTo(listener_reference.value()) &&
        (*iter)->use_capture == use_capture) {
      event_listener_infos_.erase(iter);
      return;
    }
  }
}

// Dispatch event to a single event target outside the DOM tree. The event
// propagation in the DOM tree is implemented inside Node::DispatchEvent().
bool EventTarget::DispatchEvent(const scoped_refptr<Event>& event,
                                script::ExceptionState* exception_state) {
  if (!event || event->IsBeingDispatched() || !event->initialized_flag()) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    // Return value will be ignored.
    return false;
  }

  return DispatchEvent(event);
}

bool EventTarget::DispatchEvent(const scoped_refptr<Event>& event) {
  DCHECK(event);
  DCHECK(!event->IsBeingDispatched());
  DCHECK(event->initialized_flag());

  if (!event || event->IsBeingDispatched() || !event->initialized_flag()) {
    return false;
  }

  event->set_target(this);
  event->set_event_phase(Event::kAtTarget);
  FireEventOnListeners(event);
  event->set_event_phase(Event::kNone);
  return !event->default_prevented();
}

void EventTarget::SetAttributeEventListener(
    const std::string& type, const EventListenerScriptObject& listener) {
  // Remove existing attribute listener of the same type.
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->type == type && (*iter)->listener.value().IsAttribute()) {
      event_listener_infos_.erase(iter);
      break;
    }
  }

  if (listener.IsNull()) {
    return;
  }

  EventListenerScriptObject::Reference listener_reference(this, listener);

  DCHECK(listener_reference.value().IsAttribute());
  if (!listener_reference.value().IsAttribute()) {
    return;
  }

  AddEventListenerInternal(type, listener, false);
}

const EventTarget::EventListenerScriptObject*
EventTarget::GetAttributeEventListener(const std::string& type) const {
  for (EventListenerInfos::const_iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->listener.value().IsAttribute() && (*iter)->type == type) {
      return &(*iter)->listener.referenced_object();
    }
  }
  return NULL;
}

bool EventTarget::ShouldKeepWrapperAlive() {
  return !event_listener_infos_.empty();
}

void EventTarget::FireEventOnListeners(const scoped_refptr<Event>& event) {
  DCHECK(event->IsBeingDispatched());
  DCHECK(event->target());
  DCHECK(!event->current_target());

  event->set_current_target(this);

  EventListenerInfos event_listener_infos;
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    event_listener_infos.push_back(new EventListenerInfo(
        (*iter)->type, this, (*iter)->listener.referenced_object(),
        (*iter)->use_capture));
  }

  for (EventListenerInfos::iterator iter = event_listener_infos.begin();
       iter != event_listener_infos.end(); ++iter) {
    if (event->immediate_propagation_stopped()) {
      continue;
    }
    if ((*iter)->type != event->type()) {
      continue;
    }
    // Only call listeners marked as capture during capturing phase.
    if (event->event_phase() == Event::kCapturingPhase &&
        !(*iter)->use_capture) {
      continue;
    }
    // Don't call any listeners marked as capture during bubbling phase.
    if (event->event_phase() == Event::kBubblingPhase && (*iter)->use_capture) {
      continue;
    }
    (*iter)->listener.value().HandleEvent(event);
  }

  event->set_current_target(NULL);
}

void EventTarget::AddEventListenerInternal(
    const std::string& type, const EventListenerScriptObject& listener,
    bool use_capture) {
  DCHECK(!listener.IsNull());

  EventListenerScriptObject::Reference listener_reference(this, listener);

  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->type == type &&
        (*iter)->listener.value().EqualTo(listener_reference.value()) &&
        (*iter)->use_capture == use_capture) {
      return;
    }
  }

  event_listener_infos_.push_back(
      new EventListenerInfo(type, this, listener, use_capture));
}

EventTarget::EventListenerInfo::EventListenerInfo(
    const std::string& type, const EventTarget* const event_target,
    const EventListenerScriptObject& listener, bool use_capture)
    : type(type), listener(event_target, listener), use_capture(use_capture) {}

}  // namespace dom
}  // namespace cobalt
