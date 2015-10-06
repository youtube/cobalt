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
#include "base/memory/weak_ptr.h"
#include "base/string_piece.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/event_listener.h"
#include "cobalt/script/exception_state.h"
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
  // Web API: EventTarget
  //
  void AddEventListener(const std::string& type,
                        const scoped_refptr<EventListener>& listener,
                        bool use_capture);
  void RemoveEventListener(const std::string& type,
                           const scoped_refptr<EventListener>& listener,
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
  scoped_refptr<EventListener> onblur() {
    return GetAttributeEventListener("blur");
  }
  void set_onblur(const scoped_refptr<EventListener>& event_listener) {
    SetAttributeEventListener("blur", event_listener);
  }

  scoped_refptr<EventListener> onerror() {
    return GetAttributeEventListener("error");
  }
  void set_onerror(const scoped_refptr<EventListener>& event_listener) {
    SetAttributeEventListener("error", event_listener);
  }

  scoped_refptr<EventListener> onfocus() {
    return GetAttributeEventListener("focus");
  }
  void set_onfocus(const scoped_refptr<EventListener>& event_listener) {
    SetAttributeEventListener("focus", event_listener);
  }

  scoped_refptr<EventListener> onload() {
    return GetAttributeEventListener("load");
  }
  void set_onload(const scoped_refptr<EventListener>& event_listener) {
    SetAttributeEventListener("load", event_listener);
  }

  scoped_refptr<EventListener> onreadystatechange() {
    return GetAttributeEventListener("readystatechange");
  }
  void set_onreadystatechange(
      const scoped_refptr<EventListener>& event_listener) {
    SetAttributeEventListener("readystatechange", event_listener);
  }


  // Set an event listener assigned as an attribute. Overwrite the existing one
  // if there is any.
  void SetAttributeEventListener(const std::string& type,
                                 const scoped_refptr<EventListener>& listener);

  // Get the event listener currently assigned to an attribute, or NULL if
  // there is none.
  scoped_refptr<EventListener> GetAttributeEventListener(
      const std::string& type);

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
    std::string type;
    scoped_refptr<EventListener> listener;
    bool use_capture;
  };
  typedef std::vector<EventListenerInfo> EventListenerInfos;

  void AddEventListenerInternal(const std::string& type,
                                const scoped_refptr<EventListener>& listener,
                                bool use_capture);

  EventListenerInfos event_listener_infos_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_EVENT_TARGET_H_
