// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/event_target.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/xhr/xml_http_request_event_target.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace dom {

EventTarget::EventTarget(
    script::EnvironmentSettings* settings,
    UnpackOnErrorEventsBool onerror_event_parameter_handling)
    : debugger_hooks_(
          base::polymorphic_downcast<DOMSettings*>(settings)->debugger_hooks()),
      unpack_onerror_events_(onerror_event_parameter_handling ==
                             kUnpackOnErrorEvents) {
  DCHECK(debugger_hooks_);
}

void EventTarget::AddEventListener(const std::string& type,
                                   const EventListenerScriptValue& listener,
                                   bool use_capture) {
  // Do nothing if listener is null.
  if (listener.IsNull()) {
    return;
  }

  AddEventListenerInternal(base::WrapUnique(new GenericEventHandlerReference(
      this, base::Token(type), GenericEventHandlerReference::kAddEventListener,
      use_capture, listener)));
}

void EventTarget::RemoveEventListener(const std::string& type,
                                      const EventListenerScriptValue& listener,
                                      bool use_capture) {
  // Do nothing if listener is null.
  if (listener.IsNull()) {
    return;
  }

  GenericEventHandlerReference event_handler(
      this, base::Token(type), GenericEventHandlerReference::kAddEventListener,
      use_capture, listener);
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->EqualTo(event_handler)) {
      debugger_hooks_->AsyncTaskCanceled((*iter)->task());
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
  TRACE_EVENT1("cobalt::dom", "EventTarget::DispatchEvent", "event",
               event->type().c_str());
  if (!event || event->IsBeingDispatched() || !event->initialized_flag()) {
    return false;
  }

  // The event is now being dispatched. Track it in the global stats.
  GlobalStats::GetInstance()->StartJavaScriptEvent();

  event->set_target(this);
  event->set_event_phase(Event::kAtTarget);
  FireEventOnListeners(event);
  event->set_event_phase(Event::kNone);

  // The event has completed being dispatched. Stop tracking it in the global
  // stats.
  GlobalStats::GetInstance()->StopJavaScriptEvent();

  return !event->default_prevented();
}

void EventTarget::DispatchEventAndRunCallback(
    const scoped_refptr<Event>& event,
    const base::Closure& dispatched_callback) {
  DispatchEvent(event);
  if (!dispatched_callback.is_null()) {
    dispatched_callback.Run();
  }
}

void EventTarget::DispatchEventNameAndRunCallback(
    base::Token event_name, const base::Closure& dispatched_callback) {
  DispatchEvent(base::WrapRefCounted(new Event(event_name)));
  if (!dispatched_callback.is_null()) {
    dispatched_callback.Run();
  }
}

void EventTarget::PostToDispatchEventName(const base::Location& location,
                                          base::Token event_name) {
  PostToDispatchEventNameAndRunCallback(location, event_name, base::Closure());
}

void EventTarget::PostToDispatchEvent(const base::Location& location,
                                      const scoped_refptr<Event>& event) {
  PostToDispatchEventAndRunCallback(location, event, base::Closure());
}

void EventTarget::PostToDispatchEventAndRunCallback(
    const base::Location& location, const scoped_refptr<Event>& event,
    const base::Closure& callback) {
  if (!base::MessageLoop::current()) {
    return;
  }
  base::MessageLoop::current()->task_runner()->PostTask(
      location,
      base::Bind(base::IgnoreResult(&EventTarget::DispatchEventAndRunCallback),
                 base::AsWeakPtr<EventTarget>(this), event, callback));
}

void EventTarget::PostToDispatchEventNameAndRunCallback(
    const base::Location& location, base::Token event_name,
    const base::Closure& callback) {
  if (!base::MessageLoop::current()) {
    return;
  }
  base::MessageLoop::current()->task_runner()->PostTask(
      location,
      base::Bind(
          base::IgnoreResult(&EventTarget::DispatchEventNameAndRunCallback),
          base::AsWeakPtr<EventTarget>(this), event_name, callback));
}

void EventTarget::SetAttributeEventListener(
    base::Token type, const EventListenerScriptValue& listener) {
  DCHECK(!unpack_onerror_events_ || type != base::Tokens::error());

  AddEventListenerInternal(base::WrapUnique(new GenericEventHandlerReference(
      this, type, GenericEventHandlerReference::kSetAttribute,
      false /* use_capture */, listener)));
}

const EventTarget::EventListenerScriptValue*
EventTarget::GetAttributeEventListener(base::Token type) const {
  DCHECK(!unpack_onerror_events_ || type != base::Tokens::error());

  GenericEventHandlerReference* handler =
      GetAttributeEventListenerInternal(type);
  return handler ? handler->event_listener_value() : NULL;
}

void EventTarget::SetAttributeOnErrorEventListener(
    base::Token type, const OnErrorEventListenerScriptValue& listener) {
  DCHECK_EQ(base::Tokens::error(), type);

  AddEventListenerInternal(base::WrapUnique(new GenericEventHandlerReference(
      this, type, GenericEventHandlerReference::kSetAttribute,
      false /* use_capture */, unpack_onerror_events_, listener)));
}

const EventTarget::OnErrorEventListenerScriptValue*
EventTarget::GetAttributeOnErrorEventListener(base::Token type) const {
  DCHECK_EQ(base::Tokens::error(), type);

  GenericEventHandlerReference* handler =
      GetAttributeEventListenerInternal(type);
  return handler ? handler->on_error_event_listener_value() : NULL;
}

bool EventTarget::HasOneOrMoreAttributeEventListener() const {
  for (EventListenerInfos::const_iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->is_attribute()) {
      return true;
    }
  }
  return false;
}

GenericEventHandlerReference* EventTarget::GetAttributeEventListenerInternal(
    base::Token type) const {
  for (EventListenerInfos::const_iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->is_attribute() && (*iter)->type() == type) {
      return iter->get();
    }
  }
  return NULL;
}

void EventTarget::FireEventOnListeners(const scoped_refptr<Event>& event) {
  DCHECK(event->IsBeingDispatched());
  DCHECK(event->target());
  DCHECK(!event->current_target());

  event->set_current_target(this);

  EventListenerInfos event_listener_infos;
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->type() == event->type()) {
      event_listener_infos.emplace_back(
          base::WrapUnique(new GenericEventHandlerReference(this, **iter)));
    }
  }

  for (EventListenerInfos::iterator iter = event_listener_infos.begin();
       iter != event_listener_infos.end(); ++iter) {
    if (event->immediate_propagation_stopped()) {
      continue;
    }
    // Only call listeners marked as capture during capturing phase.
    if (event->event_phase() == Event::kCapturingPhase &&
        !(*iter)->use_capture()) {
      continue;
    }
    // Don't call any listeners marked as capture during bubbling phase.
    if (event->event_phase() == Event::kBubblingPhase &&
        (*iter)->use_capture()) {
      continue;
    }

    base::ScopedAsyncTask async_task(debugger_hooks_, (*iter)->task());
    (*iter)->HandleEvent(event);
  }

  event->set_current_target(NULL);
}

void EventTarget::TraceMembers(script::Tracer* tracer) {
  SB_UNREFERENCED_PARAMETER(tracer);
  // TODO: EventListenerInfo references can be removed and logically live here
  // instead.
}

void EventTarget::AddEventListenerInternal(
    std::unique_ptr<GenericEventHandlerReference> listener) {
  TRACK_MEMORY_SCOPE("DOM");

  // Remove existing attribute listener of the same type.
  if (listener->is_attribute()) {
    for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
         iter != event_listener_infos_.end(); ++iter) {
      if ((*iter)->is_attribute() && (*iter)->type() == listener->type()) {
        debugger_hooks_->AsyncTaskCanceled((*iter)->task());
        event_listener_infos_.erase(iter);
        break;
      }
    }
  }

  if (listener->IsNull()) {
    return;
  }

  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->EqualTo(*listener)) {
      // Attribute listeners should have already been removed.
      DCHECK(!listener->is_attribute());
      return;
    }
  }

  debugger_hooks_->AsyncTaskScheduled(
      listener->task(), listener->type().c_str(), true /*recurring*/);
  event_listener_infos_.push_back(std::move(listener));
}

bool EventTarget::HasEventListener(base::Token type) {
  TRACK_MEMORY_SCOPE("DOM");

  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->type() == type) {
      return true;
    }
  }
  return false;
}

}  // namespace dom
}  // namespace cobalt
