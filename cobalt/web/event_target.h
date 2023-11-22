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

#ifndef COBALT_WEB_EVENT_TARGET_H_
#define COBALT_WEB_EVENT_TARGET_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/event.h"
#include "cobalt/web/event_listener.h"
#include "cobalt/web/event_target_listener_info.h"
#include "cobalt/web/on_error_event_listener.h"

namespace cobalt {
namespace web {

// The EventTarget interface represents an object that holds event listeners
// and possibly generates events.
// This interface describes methods and properties common to all kinds of
// EventTarget.
//   https://www.w3.org/TR/2014/WD-dom-20140710/#eventtarget
class EventTarget : public script::Wrappable,
                    public base::SupportsWeakPtr<EventTarget> {
 public:
  // Helper enum to decide whether or not onerror event parameters should be
  // unpacked or not (e.g. in the special case of the |window| object).
  // This special handling is described in:
  //   https://html.spec.whatwg.org/#event-handler-attributes
  // (search for "onerror")
  enum UnpackOnErrorEventsBool {
    kUnpackOnErrorEvents,
    kDoNotUnpackOnErrorEvents,
  };

  // The parameter |unpack_onerror_events| can be set to true (e.g. for the
  // |window| object) in order to indicate that the ErrorEvent should have
  // its members unpacked before calling its event handler.  This is to
  // accommodate for a special case in the window.onerror handling.
  explicit EventTarget(
      script::EnvironmentSettings* settings,
      UnpackOnErrorEventsBool onerror_event_parameter_handling =
          kDoNotUnpackOnErrorEvents);
  explicit EventTarget(
      web::EnvironmentSettings* settings,
      UnpackOnErrorEventsBool onerror_event_parameter_handling =
          kDoNotUnpackOnErrorEvents);

  typedef script::ScriptValue<EventListener> EventListenerScriptValue;
  typedef script::ScriptValue<OnErrorEventListener>
      OnErrorEventListenerScriptValue;

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

  // This version of DispatchEvent is intended to be called inside C++ code.  It
  // won't raise any exception.  Any error will be silently ignored.
  void DispatchEventAndRunCallback(const scoped_refptr<Event>& event,
                                   const base::Closure& dispatched_callback);

  // Creates a new event with the given name and calls DispatchEvent with it,
  // and runs dispatched_callback after finish.
  void DispatchEventNameAndRunCallback(
      base::Token event_name, const base::Closure& dispatched_callback);

  // Posts a task on the current message loop to dispatch event by name. It
  // does nothing if there is no current message loop.
  void PostToDispatchEventName(const base::Location& location,
                               base::Token event_name);

  // Posts a task on the current message loop to dispatch event. It does nothing
  // if there is no current message loop.
  void PostToDispatchEvent(const base::Location& location,
                           const scoped_refptr<Event>& event);

  // Posts a task on the current message loop to dispatch event by name, and
  // runs dispatched_callback after finish. It does nothing if there is no
  // current message loop.
  void PostToDispatchEventNameAndRunCallback(
      const base::Location& location, base::Token event_name,
      const base::Closure& dispatched_callback);

  // Posts a task on the current message loop to dispatch event, and runs
  // dispatched_callback after finish.  It does nothing if there is no current
  // message loop.
  void PostToDispatchEventAndRunCallback(
      const base::Location& location, const scoped_refptr<Event>& event,
      const base::Closure& dispatched_callback);

  // Check if target has event listener (attribute or not attribute).
  bool HasEventListener(base::Token type);

  // Web API: GlobalEventHandlers (implements)
  // Many objects can have event handlers specified. These act as non-capture
  // event listeners for the object on which they are specified.
  //   https://www.w3.org/TR/html50/webappapis.html#globaleventhandlers
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

  const OnErrorEventListenerScriptValue* onerror() {
    return GetAttributeOnErrorEventListener(base::Tokens::error());
  }
  void set_onerror(const OnErrorEventListenerScriptValue& event_listener) {
    SetAttributeOnErrorEventListener(base::Tokens::error(), event_listener);
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

  const EventListenerScriptValue* onlanguagechange() {
    return GetAttributeEventListener(base::Tokens::languagechange());
  }
  void set_onlanguagechange(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::languagechange(), event_listener);
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

  const EventListenerScriptValue* onscroll() {
    return GetAttributeEventListener(base::Tokens::scroll());
  }
  void set_onscroll(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::scroll(), event_listener);
  }

  const EventListenerScriptValue* onpointercancel() {
    return GetAttributeEventListener(base::Tokens::pointercancel());
  }
  void set_onpointercancel(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::pointercancel(), event_listener);
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

  const EventListenerScriptValue* onmessage() {
    return GetAttributeEventListener(base::Tokens::message());
  }
  void set_onmessage(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::message(), event_listener);
  }
  const EventListenerScriptValue* onmessageerror() {
    return GetAttributeEventListener(base::Tokens::messageerror());
  }
  void set_onmessageerror(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::messageerror(), event_listener);
  }

  const EventListenerScriptValue* onoffline() {
    return GetAttributeEventListener(base::Tokens::offline());
  }
  void set_onoffline(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::offline(), event_listener);
  }

  const EventListenerScriptValue* ononline() {
    return GetAttributeEventListener(base::Tokens::online());
  }
  void set_ononline(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::online(), event_listener);
  }

  const EventListenerScriptValue* onrejectionhandled() {
    return GetAttributeEventListener(base::Tokens::rejectionhandled());
  }
  void set_onrejectionhandled(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::rejectionhandled(), event_listener);
  }
  const EventListenerScriptValue* onunhandledrejection() {
    return GetAttributeEventListener(base::Tokens::unhandledrejection());
  }
  void set_onunhandledrejection(
      const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::unhandledrejection(),
                              event_listener);
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

  // Similar to SetAttributeEventListener(), but this function should only
  // be called with type == base::Tokens::error().
  void SetAttributeOnErrorEventListener(
      base::Token type, const OnErrorEventListenerScriptValue& listener);

  // Similar to GetAttributeEventListener(), but this function should only
  // be called with type == base::Tokens::error().
  const OnErrorEventListenerScriptValue* GetAttributeOnErrorEventListener(
      base::Token type) const;

  // Return true if one or more event listeners are registered
  bool HasOneOrMoreAttributeEventListener() const;

  // Returns a string that represents the target for debug purpose.
  virtual std::string GetDebugName() { return ""; }

  // This function sends the event to the event listeners attached to the
  // current event target. It takes stop immediate propagation flag into
  // account. The caller should set the phase and target.
  void FireEventOnListeners(const scoped_refptr<Event>& event);

  DEFINE_WRAPPABLE_TYPE(EventTarget);
  void TraceMembers(script::Tracer* tracer) override;

  const base::DebuggerHooks& debugger_hooks() const {
    return environment_settings_->debugger_hooks();
  }
  web::EnvironmentSettings* environment_settings() const {
    return environment_settings_;
  }
  std::set<base::Token>& event_listener_event_types() const {
    static std::set<base::Token> event_listener_event_types;
    for (auto& event_listener_info : event_listener_infos_) {
      event_listener_event_types.insert(event_listener_info->type());
    }
    return event_listener_event_types;
  }

  // Register a callback to be called when an event listener is added for the
  // given type.
  void AddEventListenerRegistrationCallback(void* object, base::Token type,
                                            base::OnceClosure callback);
  void RemoveEventListenerRegistrationCallbacks(void* object);

 protected:
  virtual ~EventTarget() { environment_settings_ = nullptr; }

 private:
  typedef std::vector<std::unique_ptr<EventTargetListenerInfo>>
      EventListenerInfos;

  void SetAttributeEventListenerInternal(
      std::unique_ptr<EventTargetListenerInfo> event_handler);
  EventTargetListenerInfo* GetAttributeEventListenerInternal(
      base::Token type) const;

  void AddEventListenerInternal(
      std::unique_ptr<EventTargetListenerInfo> listener);

  EventListenerInfos event_listener_infos_;

  web::EnvironmentSettings* environment_settings_ = nullptr;

  // Tracks whether this current event listener should unpack the onerror
  // event object when calling its callback.  This is needed to implement
  // the special case of window.onerror handling.
  bool unpack_onerror_events_;

  base::Lock event_listener_registration_mutex_;
  std::map<base::Token, std::map<void*, base::OnceClosure>>
      event_listener_registration_callbacks_;

  // Thread checker ensures all calls to the EventTarget are made from the
  // same thread that it is created in.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_EVENT_TARGET_H_
