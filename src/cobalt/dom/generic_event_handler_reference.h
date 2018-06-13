// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_GENERIC_EVENT_HANDLER_REFERENCE_H_
#define COBALT_DOM_GENERIC_EVENT_HANDLER_REFERENCE_H_

#include "base/memory/ref_counted.h"
#include "cobalt/dom/event_listener.h"
#include "cobalt/dom/on_error_event_listener.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// Essentially acts as an abstract interface of the union for types
// [script::ScriptValue<EventListener>,
//  script::ScriptValue<OnErrorEventListener>].  In particular it primarily
// allows code in event_target.cc to not need to concern itself with which
// exact EventListener script value type it is dealing with.  The need for
// this abstraction arises from the fact that the |window.onerror| event
// handler requires special case handling:
//   https://html.spec.whatwg.org/#onerroreventhandler )
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
class GenericEventHandlerReference {
 public:
  typedef script::ScriptValue<EventListener> EventListenerScriptValue;
  typedef script::ScriptValue<OnErrorEventListener>
      OnErrorEventListenerScriptValue;

  GenericEventHandlerReference(script::Wrappable* wrappable,
                               const EventListenerScriptValue& script_value);
  GenericEventHandlerReference(
      script::Wrappable* wrappable,
      const OnErrorEventListenerScriptValue& script_value);
  GenericEventHandlerReference(script::Wrappable* wrappable,
                               const GenericEventHandlerReference& other);

  // Forwards on to the internal event handler's HandleEvent() call, passing
  // in the value of |unpack_error_event| if the internal type is a
  // OnErrorEventListenerScriptValue type.
  void HandleEvent(const scoped_refptr<Event>& event, bool is_attribute,
                   bool unpack_error_event);

  bool EqualTo(const EventListenerScriptValue& other);
  bool EqualTo(const GenericEventHandlerReference& other);
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
  // At most only one of the below two fields may be non-null...  They are
  // serving as a poor man's std::variant.
  scoped_ptr<EventListenerScriptValue::Reference> event_listener_reference_;
  scoped_ptr<OnErrorEventListenerScriptValue::Reference>
      on_error_event_listener_reference_;

  DISALLOW_COPY_AND_ASSIGN(GenericEventHandlerReference);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_GENERIC_EVENT_HANDLER_REFERENCE_H_
