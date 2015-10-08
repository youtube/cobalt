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

#ifndef DOM_EVENT_TARGET_H_
#define DOM_EVENT_TARGET_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/string_piece.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/event_listener.h"
#include "cobalt/dom/event_names.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/script_object.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The EventTarget interface represents an object that holds event listeners
// and possibly generates events.
// This interface describes methods and properties common to all kinds of
// EventTarget.
//   http://www.w3.org/TR/2014/WD-dom-20140710/#eventtarget
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

  // Web API: GlobalEventHandlers (implements)
  // Many objects can have event handlers specified. These act as non-capture
  // event listeners for the object on which they are specified.
  //   http://www.w3.org/TR/html5/webappapis.html#globaleventhandlers
  //
  const EventListenerScriptObject* onblur() {
    return GetAttributeEventListener(EventNames::GetInstance()->blur());
  }
  void set_onblur(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(EventNames::GetInstance()->blur(),
                              event_listener);
  }

  const EventListenerScriptObject* onerror() {
    return GetAttributeEventListener(EventNames::GetInstance()->error());
  }
  void set_onerror(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(EventNames::GetInstance()->error(),
                              event_listener);
  }

  const EventListenerScriptObject* onfocus() {
    return GetAttributeEventListener(EventNames::GetInstance()->focus());
  }
  void set_onfocus(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(EventNames::GetInstance()->focus(),
                              event_listener);
  }

  const EventListenerScriptObject* onload() {
    return GetAttributeEventListener(EventNames::GetInstance()->load());
  }
  void set_onload(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(EventNames::GetInstance()->load(),
                              event_listener);
  }

  const EventListenerScriptObject* onreadystatechange() const {
    return GetAttributeEventListener(
        EventNames::GetInstance()->readystatechange());
  }
  void set_onreadystatechange(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(EventNames::GetInstance()->readystatechange(),
                              event_listener);
  }


  // Set an event listener assigned as an attribute. Overwrite the existing one
  // if there is any.
  void SetAttributeEventListener(const std::string& type,
                                 const EventListenerScriptObject& listener);

  // Get the event listener currently assigned to an attribute, or NULL if
  // there is none.
  const EventListenerScriptObject* GetAttributeEventListener(
      const std::string& type) const;

  // script::Wrappable
  //
  bool ShouldKeepWrapperAlive() OVERRIDE;

  DEFINE_WRAPPABLE_TYPE(EventTarget);

 protected:
  // This function sends the event to the event listeners attached to the
  // current event target. It takes stop immediate propagation flag into
  // account. The caller should set the phase and target.
  void FireEventOnListeners(const scoped_refptr<Event>& event);

 private:
  struct EventListenerInfo {
    EventListenerInfo(const std::string& type,
                      const EventTarget* const event_target,
                      const EventListenerScriptObject& listener,
                      bool use_capture);
    std::string type;
    script::ScriptObject<EventListener>::Reference listener;
    bool use_capture;
  };
  typedef ScopedVector<EventListenerInfo> EventListenerInfos;

  void AddEventListenerInternal(const std::string& type,
                                const EventListenerScriptObject& listener,
                                bool use_capture);

  EventListenerInfos event_listener_infos_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_EVENT_TARGET_H_
