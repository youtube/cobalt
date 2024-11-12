// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_dispatcher.h"

#include "base/ranges/algorithm.h"
#include "ui/events/event_target.h"
#include "ui/events/event_targeter.h"

namespace ui {

namespace {

class ScopedDispatchHelper : public Event::DispatcherApi {
 public:
  explicit ScopedDispatchHelper(Event* event)
      : Event::DispatcherApi(event) {
    set_result(ui::ER_UNHANDLED);
  }

  ScopedDispatchHelper(const ScopedDispatchHelper&) = delete;
  ScopedDispatchHelper& operator=(const ScopedDispatchHelper&) = delete;

  virtual ~ScopedDispatchHelper() {
    set_phase(EP_POSTDISPATCH);
  }
};

}  // namespace

EventDispatcherDelegate::EventDispatcherDelegate() : dispatcher_(nullptr) {}

EventDispatcherDelegate::~EventDispatcherDelegate() {
  if (dispatcher_)
    dispatcher_->OnDispatcherDelegateDestroyed();
}

Event* EventDispatcherDelegate::current_event() {
  return dispatcher_ ? dispatcher_->current_event() : NULL;
}

EventDispatchDetails EventDispatcherDelegate::DispatchEvent(EventTarget* target,
                                                            Event* event) {
  CHECK(target);
  Event::DispatcherApi dispatch_helper(event);
  dispatch_helper.set_phase(EP_PREDISPATCH);
  dispatch_helper.set_result(ER_UNHANDLED);

  EventDispatchDetails details = PreDispatchEvent(target, event);
  if (!event->handled() &&
      !details.dispatcher_destroyed &&
      !details.target_destroyed) {
    details = DispatchEventToTarget(target, event);
  }
  bool target_destroyed_during_dispatch = details.target_destroyed;
  if (!details.dispatcher_destroyed) {
    details = PostDispatchEvent(target_destroyed_during_dispatch ?
        NULL : target, *event);
  }

  details.target_destroyed |= target_destroyed_during_dispatch;
  return details;
}

EventDispatchDetails EventDispatcherDelegate::PreDispatchEvent(
    EventTarget* target, Event* event) {
  return EventDispatchDetails();
}

EventDispatchDetails EventDispatcherDelegate::PostDispatchEvent(
    EventTarget* target, const Event& event) {
  return EventDispatchDetails();
}

EventDispatchDetails EventDispatcherDelegate::DispatchEventToTarget(
    EventTarget* target,
    Event* event) {
  EventDispatcher* old_dispatcher = dispatcher_;
  EventDispatcher dispatcher(this);
  dispatcher_ = &dispatcher;
  dispatcher.ProcessEvent(target, event);
  if (!dispatcher.delegate_destroyed())
    dispatcher_ = old_dispatcher;
  else if (old_dispatcher)
    old_dispatcher->OnDispatcherDelegateDestroyed();

  EventDispatchDetails details;
  details.dispatcher_destroyed = dispatcher.delegate_destroyed();
  details.target_destroyed =
      (!details.dispatcher_destroyed && !CanDispatchToTarget(target));
  return details;
}

////////////////////////////////////////////////////////////////////////////////
// EventDispatcher:

EventDispatcher::EventDispatcher(EventDispatcherDelegate* delegate)
    : delegate_(delegate) {}

EventDispatcher::~EventDispatcher() {
  // |handler_list_| must be empty, otherwise this has been added to an
  // EventHandler that will callback to this when destroyed.
  CHECK(handler_list_.empty());
}

void EventDispatcher::OnHandlerDestroyed(EventHandler* handler) {
  handler_list_.erase(base::ranges::find(handler_list_, handler));
}

void EventDispatcher::ProcessEvent(EventTarget* target, Event* event) {
  if (!target || !target->CanAcceptEvent(*event))
    return;

  ScopedDispatchHelper dispatch_helper(event);
  dispatch_helper.set_target(target);

  handler_list_.clear();
  target->GetPreTargetHandlers(&handler_list_);

  dispatch_helper.set_phase(EP_PRETARGET);
  DispatchEventToEventHandlers(&handler_list_, event);
  // All pre-target handler should have been removed.
  CHECK(handler_list_.empty());
  if (event->handled())
    return;

  // If the event hasn't been consumed, trigger the default handler. Note that
  // even if the event has already been handled (i.e. return result has
  // ER_HANDLED set), that means that the event should still be processed at
  // this layer, however it should not be processed in the next layer of
  // abstraction.
  if (delegate_ && delegate_->CanDispatchToTarget(target) &&
      target->target_handler()) {
    dispatch_helper.set_phase(EP_TARGET);
    DispatchEvent(target->target_handler(), event);
    if (event->handled())
      return;
  }

  if (!delegate_ || !delegate_->CanDispatchToTarget(target))
    return;

  target->GetPostTargetHandlers(&handler_list_);
  dispatch_helper.set_phase(EP_POSTTARGET);
  DispatchEventToEventHandlers(&handler_list_, event);
}

void EventDispatcher::OnDispatcherDelegateDestroyed() {
  delegate_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// EventDispatcher, private:

void EventDispatcher::DispatchEventToEventHandlers(EventHandlerList* list,
                                                   Event* event) {
  // Let each EventHandler know this is about to make use of it. This way, if
  // during dispatch the EventHandler is destroyed it is removed from |list|
  // (OnHandlerDestroyed() is called, which removes from |list|).
  for (EventHandler* handler : *list)
    handler->dispatchers_.push(this);

  while (!list->empty()) {
    EventHandler* handler = (*list->begin());

    // |this| no longer needs to know if |handler| is destroyed.
    CHECK(handler->dispatchers_.top() == this);
    handler->dispatchers_.pop();
    list->erase(list->begin());

    // A null |delegate| means no more events should be dispatched.
    if (delegate_ && !event->stopped_propagation())
      DispatchEvent(handler, event);
  }
}

void EventDispatcher::DispatchEvent(EventHandler* handler, Event* event) {
  // If the target has been invalidated or deleted, don't dispatch the event.
  if (!delegate_->CanDispatchToTarget(event->target())) {
    if (event->cancelable())
      event->StopPropagation();
    return;
  }

  base::AutoReset<Event*> event_reset(&current_event_, event);
  handler->OnEvent(event);
  if (!delegate_ && event->cancelable())
    event->StopPropagation();
}

}  // namespace ui
