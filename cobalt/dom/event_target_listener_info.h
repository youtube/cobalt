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

#ifndef COBALT_DOM_EVENT_TARGET_LISTENER_INFO_H_
#define COBALT_DOM_EVENT_TARGET_LISTENER_INFO_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/event_listener.h"
#include "cobalt/dom/on_error_event_listener.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// Holds the event listener for an EventTarget, along with metadata describing
// in what manner the listener was attached to the EventTarget.
//
// The listener itself is a script::ScriptValue<T> where T may be either of:
// [EventListener, OnErrorEventListener].  In particular it primarily
// allows code in event_target.cc to not need to concern itself with which
// exact EventListener script value type it is dealing with.  The need for
// this abstraction arises from the fact that the |window.onerror| event
// handler requires special case handling:
//   https://html.spec.whatwg.org/#onerroreventhandler
//
// NOTE that this is *not* an ideal solution to the problem of generalizing
// over multiple ScriptValue types.  The problem is that the ScriptValue
// base class is both templated and abstract, making it difficult to cast its
// internal type to get a new ScriptValue.  This problem could be solved by
// refactoring ScriptValue such that there is a non-templated abstract type
// (say, RawScriptValue) with a function like "void* GetValue()", but usually
// not referenced directly by client code.  Instead there would be a separate
// templated concrete wrapper type (say, ScriptValue) that wraps RawScriptValue
// and manages the casting and type checking.  This would allow us to convert
// between OnErrorEventListener and EventListener, if OnErrorEventListener was
// derived from EventListener.
class EventTargetListenerInfo {
 public:
  typedef script::ScriptValue<EventListener> EventListenerScriptValue;
  typedef script::ScriptValue<OnErrorEventListener>
      OnErrorEventListenerScriptValue;

  // Whether an event listener is attached as an attribute or with
  // AddEventListener().
  enum AttachMethod {
    kSetAttribute,
    kAddEventListener,
  };

  EventTargetListenerInfo(script::Wrappable* wrappable, base::Token type,
                          AttachMethod attach, bool use_capture,
                          const EventListenerScriptValue& script_value);
  EventTargetListenerInfo(script::Wrappable* wrappable, base::Token type,
                          AttachMethod attach, bool use_capture,
                          bool unpack_error_event,
                          const OnErrorEventListenerScriptValue& script_value);
  EventTargetListenerInfo(script::Wrappable* wrappable,
                          const EventTargetListenerInfo& other);

  EventTargetListenerInfo(const EventTargetListenerInfo&) = delete;
  EventTargetListenerInfo& operator=(const EventTargetListenerInfo&) = delete;

  ~EventTargetListenerInfo();

  const void* task() const { return task_; }
  base::Token type() const { return type_; }
  bool is_attribute() const { return is_attribute_; }
  bool use_capture() const { return use_capture_; }

  // Forwards on to the internal event listener's HandleEvent() call, passing
  // in the value of |unpack_error_event| if the internal type is a
  // OnErrorEventListenerScriptValue type.
  void HandleEvent(const scoped_refptr<Event>& event);

  bool EqualTo(const EventTargetListenerInfo& other);
  bool IsNull() const;

  // If the internal type is a EventListenerScriptValue, then its value will
  // be returned, otherwise null is returned;
  const EventListenerScriptValue* event_listener_value() {
    return event_listener_reference_
               ? &event_listener_reference_->referenced_value()
               : nullptr;
  }

  // If the internal type is a OnErrorEventListenerScriptValue, then its value
  // will be returned, otherwise null is returned;
  const OnErrorEventListenerScriptValue* on_error_event_listener_value() {
    return on_error_event_listener_reference_
               ? &on_error_event_listener_reference_->referenced_value()
               : nullptr;
  }

 private:
  // A nonce to identify the "scheduled" asynchronous task that could call the
  // listener when the event is fired. The constructors that create a new
  // "attachment" of a listener initialize it to |this| as a unique nonce value.
  // However, the copy(ish) constructor copies the task since the copy still
  // represents the same attachment of the same listener. It is specifically NOT
  // tied to the ScriptValue since the same JS listener may be attached multiple
  // times to one or several |EventTarget|s, and each of those attachments is a
  // unique task.
  const void* const task_;

  base::Token const type_;
  bool const is_attribute_;
  bool const use_capture_;
  bool const unpack_error_event_;

  // At most only one of the below two fields may be non-null...  They are
  // serving as a poor man's std::variant.
  std::unique_ptr<EventListenerScriptValue::Reference>
      event_listener_reference_;
  std::unique_ptr<OnErrorEventListenerScriptValue::Reference>
      on_error_event_listener_reference_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EVENT_TARGET_LISTENER_INFO_H_
