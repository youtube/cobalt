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

#include "cobalt/web/event_target.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/global_stats.h"

namespace cobalt {
namespace web {

// Since EventTarget is always in a web environment, we store the environment
// settings as web environment settings, and make it easily available to all
// derived classes with the environment_settings() accessor.
EventTarget::EventTarget(
    script::EnvironmentSettings* settings,
    UnpackOnErrorEventsBool onerror_event_parameter_handling)
    : environment_settings_(
          base::polymorphic_downcast<web::EnvironmentSettings*>(settings)),
      unpack_onerror_events_(onerror_event_parameter_handling ==
                             kUnpackOnErrorEvents) {
  DCHECK(environment_settings_);
}

EventTarget::EventTarget(
    web::EnvironmentSettings* settings,
    UnpackOnErrorEventsBool onerror_event_parameter_handling)
    : environment_settings_(settings),
      unpack_onerror_events_(onerror_event_parameter_handling ==
                             kUnpackOnErrorEvents) {
  DCHECK(environment_settings_);
}

void EventTarget::AddEventListener(const std::string& type,
                                   const EventListenerScriptValue& listener,
                                   bool use_capture) {
  // Do nothing if listener is null.
  if (listener.IsNull()) {
    return;
  }

  AddEventListenerInternal(base::WrapUnique(new EventTargetListenerInfo(
      this, base::Token(type), EventTargetListenerInfo::kAddEventListener,
      use_capture, listener)));
}

void EventTarget::RemoveEventListener(const std::string& type,
                                      const EventListenerScriptValue& listener,
                                      bool use_capture) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Do nothing if listener is null.
  if (listener.IsNull()) {
    return;
  }

  EventTargetListenerInfo listener_info(
      this, base::Token(type), EventTargetListenerInfo::kAddEventListener,
      use_capture, listener);
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->EqualTo(listener_info)) {
      debugger_hooks().AsyncTaskCanceled((*iter)->task());
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
  TRACE_EVENT2("cobalt::dom", "EventTarget::DispatchEvent", "name",
               GetDebugName(), "event", TRACE_STR_COPY(event->type().c_str()));
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      location,
      base::Bind(
          base::IgnoreResult(&EventTarget::DispatchEventNameAndRunCallback),
          base::AsWeakPtr<EventTarget>(this), event_name, callback));
}

void EventTarget::SetAttributeEventListener(
    base::Token type, const EventListenerScriptValue& listener) {
  DCHECK(!unpack_onerror_events_ || type != base::Tokens::error());

  AddEventListenerInternal(base::WrapUnique(new EventTargetListenerInfo(
      this, type, EventTargetListenerInfo::kSetAttribute,
      false /* use_capture */, listener)));
}

const EventTarget::EventListenerScriptValue*
EventTarget::GetAttributeEventListener(base::Token type) const {
  DCHECK(!unpack_onerror_events_ || type != base::Tokens::error());

  EventTargetListenerInfo* listener_info =
      GetAttributeEventListenerInternal(type);
  return listener_info ? listener_info->event_listener_value() : NULL;
}

void EventTarget::SetAttributeOnErrorEventListener(
    base::Token type, const OnErrorEventListenerScriptValue& listener) {
  DCHECK_EQ(base::Tokens::error(), type);

  AddEventListenerInternal(base::WrapUnique(new EventTargetListenerInfo(
      this, type, EventTargetListenerInfo::kSetAttribute,
      false /* use_capture */, unpack_onerror_events_, listener)));
}

const EventTarget::OnErrorEventListenerScriptValue*
EventTarget::GetAttributeOnErrorEventListener(base::Token type) const {
  DCHECK_EQ(base::Tokens::error(), type);

  EventTargetListenerInfo* listener_info =
      GetAttributeEventListenerInternal(type);
  return listener_info ? listener_info->on_error_event_listener_value() : NULL;
}

bool EventTarget::HasOneOrMoreAttributeEventListener() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (EventListenerInfos::const_iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->is_attribute()) {
      return true;
    }
  }
  return false;
}

EventTargetListenerInfo* EventTarget::GetAttributeEventListenerInternal(
    base::Token type) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (EventListenerInfos::const_iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->is_attribute() && (*iter)->type() == type) {
      return iter->get();
    }
  }
  return NULL;
}

void EventTarget::FireEventOnListeners(const scoped_refptr<Event>& event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(event->IsBeingDispatched());
  DCHECK(event->target());
  DCHECK(!event->current_target());

  event->set_current_target(this);

  EventListenerInfos event_listener_infos;
  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->type() == event->type()) {
      event_listener_infos.emplace_back(
          base::WrapUnique(new EventTargetListenerInfo(this, **iter)));
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

    base::ScopedAsyncTask async_task(debugger_hooks(), (*iter)->task());
    (*iter)->HandleEvent(event);
  }

  event->set_current_target(NULL);
}

void EventTarget::TraceMembers(script::Tracer* tracer) {
  // TODO: EventListenerInfo references can be removed and logically live here
  // instead.
}

void EventTarget::AddEventListenerRegistrationCallback(
    void* object, base::Token token, base::OnceClosure callback) {
  base::AutoLock lock(event_listener_registration_mutex_);
  event_listener_registration_callbacks_[token][object] = std::move(callback);
}

void EventTarget::RemoveEventListenerRegistrationCallbacks(void* object) {
  base::AutoLock lock(event_listener_registration_mutex_);
  for (auto& token : event_listener_registration_callbacks_)
    token.second.erase(object);
}

void EventTarget::AddEventListenerInternal(
    std::unique_ptr<EventTargetListenerInfo> listener_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(listener_info);

  // Remove existing attribute listener of the same type.
  if (listener_info->is_attribute()) {
    for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
         iter != event_listener_infos_.end(); ++iter) {
      if ((*iter)->is_attribute() && (*iter)->type() == listener_info->type()) {
        debugger_hooks().AsyncTaskCanceled((*iter)->task());
        event_listener_infos_.erase(iter);
        break;
      }
    }
  }

  if (listener_info->IsNull()) {
    return;
  }

  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->EqualTo(*listener_info)) {
      // Attribute listeners should have already been removed.
      DCHECK(!listener_info->is_attribute());
      return;
    }
  }

  debugger_hooks().AsyncTaskScheduled(
      listener_info->task(), listener_info->type().c_str(),
      base::DebuggerHooks::AsyncTaskFrequency::kRecurring);

  base::Token type = listener_info->type();
  event_listener_infos_.push_back(std::move(listener_info));

  {
    // Call the event listener registration callback.
    base::AutoLock lock(event_listener_registration_mutex_);
    auto callbacks = event_listener_registration_callbacks_.find(type);
    if (callbacks != event_listener_registration_callbacks_.end()) {
      for (auto& object : callbacks->second) {
        std::move(object.second).Run();
      }
      event_listener_registration_callbacks_.erase(type);
    }
  }
}

bool EventTarget::HasEventListener(base::Token type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (EventListenerInfos::iterator iter = event_listener_infos_.begin();
       iter != event_listener_infos_.end(); ++iter) {
    if ((*iter)->type() == type) {
      return true;
    }
  }
  return false;
}

}  // namespace web
}  // namespace cobalt
