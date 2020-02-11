// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/event_target_listener_info.h"

#include "base/trace_event/trace_event.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/global_stats.h"

namespace cobalt {
namespace dom {

EventTargetListenerInfo::EventTargetListenerInfo(
    script::Wrappable* wrappable, base::Token type, AttachMethod attach,
    bool use_capture, const EventListenerScriptValue& script_value)
    : ALLOW_THIS_IN_INITIALIZER_LIST(task_(this)),
      type_(type),
      is_attribute_(attach == kSetAttribute),
      use_capture_(use_capture),
      unpack_error_event_(false) {
  if (!script_value.IsNull()) {
    GlobalStats::GetInstance()->AddEventListener();
    event_listener_reference_.reset(
        new EventListenerScriptValue::Reference(wrappable, script_value));
  }
}

EventTargetListenerInfo::EventTargetListenerInfo(
    script::Wrappable* wrappable, base::Token type, AttachMethod attach,
    bool use_capture, bool unpack_error_event,
    const OnErrorEventListenerScriptValue& script_value)
    : ALLOW_THIS_IN_INITIALIZER_LIST(task_(this)),
      type_(type),
      is_attribute_(attach == kSetAttribute),
      use_capture_(use_capture),
      unpack_error_event_(unpack_error_event) {
  if (!script_value.IsNull()) {
    GlobalStats::GetInstance()->AddEventListener();
    on_error_event_listener_reference_.reset(
        new OnErrorEventListenerScriptValue::Reference(wrappable,
                                                       script_value));
  }
}

EventTargetListenerInfo::EventTargetListenerInfo(
    script::Wrappable* wrappable, const EventTargetListenerInfo& other)
    : task_(other.task_),
      type_(other.type_),
      is_attribute_(other.is_attribute_),
      use_capture_(other.use_capture_),
      unpack_error_event_(other.unpack_error_event_) {
  if (other.event_listener_reference_) {
    DCHECK(!other.event_listener_reference_->referenced_value().IsNull());
    GlobalStats::GetInstance()->AddEventListener();
    event_listener_reference_.reset(new EventListenerScriptValue::Reference(
        wrappable, other.event_listener_reference_->referenced_value()));
  } else if (other.on_error_event_listener_reference_) {
    GlobalStats::GetInstance()->AddEventListener();
    on_error_event_listener_reference_.reset(
        new OnErrorEventListenerScriptValue::Reference(
            wrappable,
            other.on_error_event_listener_reference_->referenced_value()));
  }
}

EventTargetListenerInfo::~EventTargetListenerInfo() {
  if (event_listener_reference_.get() ||
      on_error_event_listener_reference_.get()) {
    // If any instances are destroyed after V8 shutdown, the ScriptValues
    // will already be gone, so we use the references' pointer validity
    // instead of IsNull() to test if the EventListener is added to global
    // stats.
    GlobalStats::GetInstance()->RemoveEventListener();
  }
}

void EventTargetListenerInfo::HandleEvent(const scoped_refptr<Event>& event) {
  TRACE_EVENT1("cobalt::dom", "EventTargetListenerInfo::HandleEvent",
               "Event Name", TRACE_STR_COPY(event->type().c_str()));
  bool had_exception;
  base::Optional<bool> result;

  // Forward the HandleEvent() call to the appropriate internal object.
  if (event_listener_reference_) {
    // Non-onerror event handlers cannot have their parameters unpacked.
    result = event_listener_reference_->value().HandleEvent(
        event->current_target(), event, &had_exception);
  } else if (on_error_event_listener_reference_) {
    result = on_error_event_listener_reference_->value().HandleEvent(
        event->current_target(), event, &had_exception, unpack_error_event_);
  } else {
    NOTREACHED();
    had_exception = true;
  }

  if (had_exception) {
    return;
  }
  // EventHandlers (EventListeners set as attributes) may return false rather
  // than call event.preventDefault() in the handler function.
  if (is_attribute() && result && !result.value()) {
    event->PreventDefault();
  }
}

bool EventTargetListenerInfo::EqualTo(const EventTargetListenerInfo& other) {
  if (type() != other.type() || is_attribute() != other.is_attribute() ||
      use_capture() != other.use_capture()) {
    return false;
  }

  if (IsNull() && other.IsNull()) {
    return true;
  }

  if (event_listener_reference_ && other.event_listener_reference_) {
    return event_listener_reference_->referenced_value().EqualTo(
        other.event_listener_reference_->referenced_value());
  }
  if (on_error_event_listener_reference_ &&
      other.on_error_event_listener_reference_) {
    return on_error_event_listener_reference_->referenced_value().EqualTo(
        other.on_error_event_listener_reference_->referenced_value());
  }
  return false;
}

bool EventTargetListenerInfo::IsNull() const {
  return (!event_listener_reference_ ||
          event_listener_reference_->referenced_value().IsNull()) &&
         (!on_error_event_listener_reference_ ||
          on_error_event_listener_reference_->referenced_value().IsNull());
}

}  // namespace dom
}  // namespace cobalt
