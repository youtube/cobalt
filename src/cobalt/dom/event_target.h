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

#ifndef COBALT_DOM_EVENT_TARGET_H_
#define COBALT_DOM_EVENT_TARGET_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/tracked_objects.h"
#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/event_listener.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/script_object.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The EventTarget interface represents an object that holds event listeners
// and possibly generates events.
// This interface describes methods and properties common to all kinds of
// EventTarget.
//   https://www.w3.org/TR/2014/WD-dom-20140710/#eventtarget
class EventTarget : public script::Wrappable,
                    public base::SupportsWeakPtr<EventTarget> {
 public:
  typedef script::ScriptObject<EventListener> EventListenerScriptObject;

  // Web API: EventTarget
  //
  void AddEventListener(const std::string& type,
                        const EventListenerScriptObject& listener,
                        bool use_capture);
  void RemoveEventListener(const std::string& type,
                           const EventListenerScriptObject& listener,
                           bool use_capture);
  bool DispatchEvent(const scoped_refptr<Event>& event,
                     script::ExceptionState* exception_state);

  // Custom, not in any spec.
  //
  // This version of DispatchEvent is intended to be called inside C++ code.  It
  // won't raise any exception.  Any error will be silently ignored.
  virtual bool DispatchEvent(const scoped_refptr<Event>& event);

  // Creates a new event with the given name and calls DispatchEvent with it,
  // and runs dispatched_callback after finish.
  void DispatchEventAndRunCallback(base::Token event_name,
                                   const base::Closure& dispatched_callback);

  // Posts a task on the current message loop to dispatch event. It does nothing
  // if there is no current message loop.
  void PostToDispatchEvent(const tracked_objects::Location& location,
                           base::Token event_name);

  // Posts a task on the current message loop to dispatch event, and runs
  // dispatched_callback after finish.  It does nothing if there is no current
  // message loop.
  void PostToDispatchEventAndRunCallback(
      const tracked_objects::Location& location, base::Token event_name,
      const base::Closure& dispatched_callback);

  // Web API: GlobalEventHandlers (implements)
  // Many objects can have event handlers specified. These act as non-capture
  // event listeners for the object on which they are specified.
  //   https://www.w3.org/TR/html5/webappapis.html#globaleventhandlers
  //
  const EventListenerScriptObject* onblur() {
    return GetAttributeEventListener(base::Tokens::blur());
  }
  void set_onblur(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(base::Tokens::blur(), event_listener);
  }

  const EventListenerScriptObject* onerror() {
    return GetAttributeEventListener(base::Tokens::error());
  }
  void set_onerror(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(base::Tokens::error(), event_listener);
  }

  const EventListenerScriptObject* onfocus() {
    return GetAttributeEventListener(base::Tokens::focus());
  }
  void set_onfocus(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(base::Tokens::focus(), event_listener);
  }

  const EventListenerScriptObject* onload() {
    return GetAttributeEventListener(base::Tokens::load());
  }
  void set_onload(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(base::Tokens::load(), event_listener);
  }

  const EventListenerScriptObject* onunload() {
    return GetAttributeEventListener(base::Tokens::unload());
  }
  void set_onunload(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(base::Tokens::unload(), event_listener);
  }

  // Set an event listener assigned as an attribute. Overwrite the existing one
  // if there is any.
  void SetAttributeEventListener(base::Token type,
                                 const EventListenerScriptObject& listener);

  // Get the event listener currently assigned to an attribute, or NULL if
  // there is none.
  const EventListenerScriptObject* GetAttributeEventListener(
      base::Token type) const;

  // script::Wrappable
  //
  bool ShouldKeepWrapperAlive() OVERRIDE;

  // Returns a string that represents the target for debug purpose.
  virtual std::string GetDebugName() { return ""; }

  DEFINE_WRAPPABLE_TYPE(EventTarget);

 protected:
  // This function sends the event to the event listeners attached to the
  // current event target. It takes stop immediate propagation flag into
  // account. The caller should set the phase and target.
  void FireEventOnListeners(const scoped_refptr<Event>& event);

 private:
  struct EventListenerInfo {
    EventListenerInfo(base::Token type, EventTarget* const event_target,
                      const EventListenerScriptObject& listener,
                      bool use_capture, EventListener::Type listener_type);
    ~EventListenerInfo();

    base::Token type;
    script::ScriptObject<EventListener>::Reference listener;
    bool use_capture;
    EventListener::Type listener_type;
  };
  typedef ScopedVector<EventListenerInfo> EventListenerInfos;

  void AddEventListenerInternal(base::Token type,
                                const EventListenerScriptObject& listener,
                                bool use_capture,
                                EventListener::Type listener_type);

  EventListenerInfos event_listener_infos_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EVENT_TARGET_H_
