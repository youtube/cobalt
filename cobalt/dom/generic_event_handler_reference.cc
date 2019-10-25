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

#include "cobalt/dom/generic_event_handler_reference.h"

#include "base/trace_event/trace_event.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace dom {

GenericEventHandlerReference::GenericEventHandlerReference(
    script::Wrappable* wrappable,
    const EventListenerScriptValue& script_value) {
  if (!script_value.IsNull()) {
    event_listener_reference_.reset(
        new EventListenerScriptValue::Reference(wrappable, script_value));
  }
}

GenericEventHandlerReference::GenericEventHandlerReference(
    script::Wrappable* wrappable,
    const OnErrorEventListenerScriptValue& script_value) {
  if (!script_value.IsNull()) {
    on_error_event_listener_reference_.reset(
        new OnErrorEventListenerScriptValue::Reference(wrappable,
                                                       script_value));
  }
}

GenericEventHandlerReference::GenericEventHandlerReference(
    script::Wrappable* wrappable, const GenericEventHandlerReference& other) {
  if (other.event_listener_reference_) {
    DCHECK(!other.event_listener_reference_->referenced_value().IsNull());
    event_listener_reference_.reset(new EventListenerScriptValue::Reference(
        wrappable, other.event_listener_reference_->referenced_value()));
  } else if (other.on_error_event_listener_reference_) {
    on_error_event_listener_reference_.reset(
        new OnErrorEventListenerScriptValue::Reference(
            wrappable,
            other.on_error_event_listener_reference_->referenced_value()));
  }
}

void GenericEventHandlerReference::HandleEvent(
    const scoped_refptr<Event>& event, bool is_attribute,
    bool unpack_error_event) {
  TRACE_EVENT1("cobalt::dom", "GenericEventHandlerReference::HandleEvent",
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
        event->current_target(), event, &had_exception, unpack_error_event);
  } else {
    NOTREACHED();
    had_exception = true;
  }

  if (had_exception) {
    return;
  }
  // EventHandlers (EventListeners set as attributes) may return false rather
  // than call event.preventDefault() in the handler function.
  if (is_attribute && result && !result.value()) {
    event->PreventDefault();
  }
}

bool GenericEventHandlerReference::EqualTo(
    const EventListenerScriptValue& other) {
  return (IsNull() && other.IsNull()) ||
         (event_listener_reference_ &&
          event_listener_reference_->referenced_value().EqualTo(other));
}

bool GenericEventHandlerReference::EqualTo(
    const GenericEventHandlerReference& other) {
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

bool GenericEventHandlerReference::IsNull() const {
  return (!event_listener_reference_ ||
          event_listener_reference_->referenced_value().IsNull()) &&
         (!on_error_event_listener_reference_ ||
          on_error_event_listener_reference_->referenced_value().IsNull());
}

}  // namespace dom
}  // namespace cobalt
