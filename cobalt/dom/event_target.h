// Copyright 2015 Google Inc. All Rights Reserved.
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
#include "cobalt/script/script_value.h"
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
  typedef script::ScriptValue<EventListener> EventListenerScriptValue;

  // Web API: EventTarget
  //
  void AddEventListener(const std::string& type,
                        const EventListenerScriptValue& listener,
                        bool use_capture);
  void RemoveEventListener(const std::string& type,
                           const EventListenerScriptValue& listener,
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

  // Check if target has event listener (atrtibute or not attribute).
  bool HasEventListener(base::Token type);

  // Web API: GlobalEventHandlers (implements)
  // Many objects can have event handlers specified. These act as non-capture
  // event listeners for the object on which they are specified.
  //   https://www.w3.org/TR/html5/webappapis.html#globaleventhandlers
  //
  const EventListenerScriptValue* onblur() {
    return GetAttributeEventListener(base::Tokens::blur());
  }
  void set_onblur(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::blur(), event_listener);
  }

  const EventListenerScriptValue* onclick() {
    return GetAttributeEventListener(base::Tokens::click());
  }
  void set_onclick(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::click(), event_listener);
  }

  const EventListenerScriptValue* onerror() {
    return GetAttributeEventListener(base::Tokens::error());
  }
  void set_onerror(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::error(), event_listener);
  }

  const EventListenerScriptValue* onfocus() {
    return GetAttributeEventListener(base::Tokens::focus());
  }
  void set_onfocus(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::focus(), event_listener);
  }

  const EventListenerScriptValue* onkeydown() {
    return GetAttributeEventListener(base::Tokens::keydown());
  }
  void set_onkeydown(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::keydown(), event_listener);
  }

  const EventListenerScriptValue* onkeypress() {
    return GetAttributeEventListener(base::Tokens::keypress());
  }
  void set_onkeypress(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::keypress(), event_listener);
  }

  const EventListenerScriptValue* onkeyup() {
    return GetAttributeEventListener(base::Tokens::keyup());
  }
  void set_onkeyup(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::keyup(), event_listener);
  }

  const EventListenerScriptValue* onload() {
    return GetAttributeEventListener(base::Tokens::load());
  }
  void set_onload(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::load(), event_listener);
  }

  const EventListenerScriptValue* onloadeddata() {
    return GetAttributeEventListener(base::Tokens::loadeddata());
  }
  void set_onloadeddata(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::loadeddata(), event_listener);
  }

  const EventListenerScriptValue* onloadedmetadata() {
    return GetAttributeEventListener(base::Tokens::loadedmetadata());
  }
  void set_onloadedmetadata(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::loadedmetadata(), event_listener);
  }

  const EventListenerScriptValue* onloadstart() {
    return GetAttributeEventListener(base::Tokens::loadstart());
  }
  void set_onloadstart(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::loadstart(), event_listener);
  }

  const EventListenerScriptValue* onmousedown() {
    return GetAttributeEventListener(base::Tokens::mousedown());
  }
  void set_onmousedown(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::mousedown(), event_listener);
  }

  const EventListenerScriptValue* onmouseenter() {
    return GetAttributeEventListener(base::Tokens::mouseenter());
  }
  void set_onmouseenter(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::mouseenter(), event_listener);
  }

  const EventListenerScriptValue* onmouseleave() {
    return GetAttributeEventListener(base::Tokens::mouseleave());
  }
  void set_onmouseleave(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::mouseleave(), event_listener);
  }

  const EventListenerScriptValue* onmousemove() {
    return GetAttributeEventListener(base::Tokens::mousemove());
  }
  void set_onmousemove(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::mousemove(), event_listener);
  }

  const EventListenerScriptValue* onmouseout() {
    return GetAttributeEventListener(base::Tokens::mouseout());
  }
  void set_onmouseout(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::mouseout(), event_listener);
  }

  const EventListenerScriptValue* onmouseover() {
    return GetAttributeEventListener(base::Tokens::mouseover());
  }
  void set_onmouseover(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::mouseover(), event_listener);
  }

  const EventListenerScriptValue* onmouseup() {
    return GetAttributeEventListener(base::Tokens::mouseup());
  }
  void set_onmouseup(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::mouseup(), event_listener);
  }

  const EventListenerScriptValue* onpause() {
    return GetAttributeEventListener(base::Tokens::pause());
  }
  void set_onpause(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::pause(), event_listener);
  }

  const EventListenerScriptValue* onplay() {
    return GetAttributeEventListener(base::Tokens::play());
  }
  void set_onplay(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::play(), event_listener);
  }

  const EventListenerScriptValue* onplaying() {
    return GetAttributeEventListener(base::Tokens::playing());
  }
  void set_onplaying(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::playing(), event_listener);
  }

  const EventListenerScriptValue* onresize() {
    return GetAttributeEventListener(base::Tokens::resize());
  }
  void set_onresize(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::resize(), event_listener);
  }

  const EventListenerScriptValue* ongotpointercapture() {
    return GetAttributeEventListener(base::Tokens::gotpointercapture());
  }
  void set_ongotpointercapture(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::gotpointercapture(),
                              event_listener);
  }

  const EventListenerScriptValue* onlostpointercapture() {
    return GetAttributeEventListener(base::Tokens::lostpointercapture());
  }
  void set_onlostpointercapture(
      const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::lostpointercapture(),
                              event_listener);
  }

  const EventListenerScriptValue* onpointerdown() {
    return GetAttributeEventListener(base::Tokens::pointerdown());
  }
  void set_onpointerdown(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::pointerdown(), event_listener);
  }

  const EventListenerScriptValue* onpointerenter() {
    return GetAttributeEventListener(base::Tokens::pointerenter());
  }
  void set_onpointerenter(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::pointerenter(), event_listener);
  }

  const EventListenerScriptValue* onpointerleave() {
    return GetAttributeEventListener(base::Tokens::pointerleave());
  }
  void set_onpointerleave(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::pointerleave(), event_listener);
  }

  const EventListenerScriptValue* onpointermove() {
    return GetAttributeEventListener(base::Tokens::pointermove());
  }
  void set_onpointermove(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::pointermove(), event_listener);
  }

  const EventListenerScriptValue* onpointerout() {
    return GetAttributeEventListener(base::Tokens::pointerout());
  }
  void set_onpointerout(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::pointerout(), event_listener);
  }

  const EventListenerScriptValue* onpointerover() {
    return GetAttributeEventListener(base::Tokens::pointerover());
  }
  void set_onpointerover(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::pointerover(), event_listener);
  }

  const EventListenerScriptValue* onpointerup() {
    return GetAttributeEventListener(base::Tokens::pointerup());
  }
  void set_onpointerup(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::pointerup(), event_listener);
  }

  const EventListenerScriptValue* onprogress() {
    return GetAttributeEventListener(base::Tokens::progress());
  }
  void set_onprogress(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::progress(), event_listener);
  }

  const EventListenerScriptValue* onratechange() {
    return GetAttributeEventListener(base::Tokens::ratechange());
  }
  void set_onratechange(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::ratechange(), event_listener);
  }

  const EventListenerScriptValue* onseeked() {
    return GetAttributeEventListener(base::Tokens::seeked());
  }
  void set_onseeked(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::seeked(), event_listener);
  }

  const EventListenerScriptValue* onseeking() {
    return GetAttributeEventListener(base::Tokens::seeking());
  }
  void set_onseeking(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::seeking(), event_listener);
  }

  const EventListenerScriptValue* ontimeupdate() {
    return GetAttributeEventListener(base::Tokens::timeupdate());
  }
  void set_ontimeupdate(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::timeupdate(), event_listener);
  }

  const EventListenerScriptValue* onbeforeunload() {
    return GetAttributeEventListener(base::Tokens::beforeunload());
  }
  void set_onbeforeunload(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::beforeunload(), event_listener);
  }

  const EventListenerScriptValue* ontransitionend() {
    return GetAttributeEventListener(base::Tokens::transitionend());
  }
  void set_ontransitionend(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::transitionend(), event_listener);
  }

  const EventListenerScriptValue* onunload() {
    return GetAttributeEventListener(base::Tokens::unload());
  }
  void set_onunload(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::unload(), event_listener);
  }

  const EventListenerScriptValue* onvolumechange() {
    return GetAttributeEventListener(base::Tokens::volumechange());
  }
  void set_onvolumechange(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::volumechange(), event_listener);
  }

  const EventListenerScriptValue* onwaiting() {
    return GetAttributeEventListener(base::Tokens::waiting());
  }
  void set_onwaiting(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::waiting(), event_listener);
  }

  const EventListenerScriptValue* onwheel() {
    return GetAttributeEventListener(base::Tokens::wheel());
  }
  void set_onwheel(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::wheel(), event_listener);
  }

  // Set an event listener assigned as an attribute. Overwrite the existing one
  // if there is any.
  void SetAttributeEventListener(base::Token type,
                                 const EventListenerScriptValue& listener);

  // Get the event listener currently assigned to an attribute, or NULL if
  // there is none.
  const EventListenerScriptValue* GetAttributeEventListener(
      base::Token type) const;

  // Return true if one or more event listeners are registered
  bool HasOneOrMoreAttributeEventListener() const;

  // script::Wrappable
  //
  bool ShouldKeepWrapperAlive() override;

  // Returns a string that represents the target for debug purpose.
  virtual std::string GetDebugName() { return ""; }

  void TraceMembers(script::Tracer* tracer) override;

  // This function sends the event to the event listeners attached to the
  // current event target. It takes stop immediate propagation flag into
  // account. The caller should set the phase and target.
  void FireEventOnListeners(const scoped_refptr<Event>& event);

  DEFINE_WRAPPABLE_TYPE(EventTarget);

 private:
  struct EventListenerInfo {
    EventListenerInfo(base::Token type, EventTarget* const event_target,
                      const EventListenerScriptValue& listener,
                      bool use_capture, EventListener::Type listener_type);
    ~EventListenerInfo();

    base::Token type;
    script::ScriptValue<EventListener>::Reference listener;
    bool use_capture;
    EventListener::Type listener_type;
  };
  typedef ScopedVector<EventListenerInfo> EventListenerInfos;

  void AddEventListenerInternal(base::Token type,
                                const EventListenerScriptValue& listener,
                                bool use_capture,
                                EventListener::Type listener_type);

  EventListenerInfos event_listener_infos_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EVENT_TARGET_H_
