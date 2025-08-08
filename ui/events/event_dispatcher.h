// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_DISPATCHER_H_
#define UI_EVENTS_EVENT_DISPATCHER_H_

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/events_export.h"

namespace ui {

class EventDispatcher;
class EventTarget;

struct EventDispatchDetails {
  bool dispatcher_destroyed = false;
  bool target_destroyed = false;

  // Set to true if an EventRewriter discards the event.
  bool event_discarded = false;
};

class EVENTS_EXPORT EventDispatcherDelegate {
 public:
  EventDispatcherDelegate();

  EventDispatcherDelegate(const EventDispatcherDelegate&) = delete;
  EventDispatcherDelegate& operator=(const EventDispatcherDelegate&) = delete;

  virtual ~EventDispatcherDelegate();

  // Returns whether an event can still be dispatched to a target. (e.g. during
  // event dispatch, one of the handlers may have destroyed the target, in which
  // case the event can no longer be dispatched to the target).
  virtual bool CanDispatchToTarget(EventTarget* target) = 0;

  // Returns the event being dispatched (or NULL if no event is being
  // dispatched).
  Event* current_event();

  // Dispatches |event| to |target|. This calls |PreDispatchEvent()| before
  // dispatching the event, and |PostDispatchEvent()| after the event has been
  // dispatched.
  [[nodiscard]] EventDispatchDetails DispatchEvent(EventTarget* target,
                                                   Event* event);

 protected:
  // This is called once a target has been determined for an event, right before
  // the event is dispatched to the target. This function may modify |event| to
  // prepare it for dispatch (e.g. update event flags, location etc.).
  [[nodiscard]] virtual EventDispatchDetails PreDispatchEvent(
      EventTarget* target,
      Event* event);

  // This is called right after the event dispatch is completed.
  // |target| is NULL if the target was deleted during dispatch.
  [[nodiscard]] virtual EventDispatchDetails PostDispatchEvent(
      EventTarget* target,
      const Event& event);

 private:
  // Dispatches the event to the target.
  [[nodiscard]] EventDispatchDetails DispatchEventToTarget(EventTarget* target,
                                                           Event* event);

  raw_ptr<EventDispatcher> dispatcher_;
};

// Dispatches events to appropriate targets.
class EVENTS_EXPORT EventDispatcher {
 public:
  explicit EventDispatcher(EventDispatcherDelegate* delegate);

  EventDispatcher(const EventDispatcher&) = delete;
  EventDispatcher& operator=(const EventDispatcher&) = delete;

  virtual ~EventDispatcher();

  void ProcessEvent(EventTarget* target, Event* event);

  const Event* current_event() const { return current_event_; }
  Event* current_event() { return current_event_; }

  bool delegate_destroyed() const { return !delegate_; }

  void OnHandlerDestroyed(EventHandler* handler);
  void OnDispatcherDelegateDestroyed();

 private:
  void DispatchEventToEventHandlers(EventHandlerList* list, Event* event);

  // Dispatches an event, and makes sure it sets ER_CONSUMED on the
  // event-handling result if the dispatcher itself has been destroyed during
  // dispatching the event to the event handler.
  void DispatchEvent(EventHandler* handler, Event* event);

  raw_ptr<EventDispatcherDelegate> delegate_;

  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Event* current_event_ = nullptr;

  EventHandlerList handler_list_;
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_DISPATCHER_H_
