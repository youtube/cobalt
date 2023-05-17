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

#include "starboard/shared/starboard/application.h"

#include <string>

#include "starboard/atomic.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/log.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/event.h"
#include "starboard/memory.h"

#include "starboard/shared/starboard/command_line.h"

namespace starboard {
namespace shared {
namespace starboard {

namespace {

const char kPreloadSwitch[] = "preload";
const char kLinkSwitch[] = "link";
const char kMinLogLevel[] = "min_log_level";

// Dispatches an event of |type| with |data| to the system event handler,
// calling |destructor| on |data| when finished dispatching. Does all
// appropriate NULL checks so you don't have to.
void Dispatch(SbEventType type, void* data, SbEventDataDestructor destructor) {
  SbEvent event;
  event.type = type;
  event.data = data;
#if SB_API_VERSION >= 15
  Application::Get()->sb_event_handle_callback_(&event);
#else
  SbEventHandle(&event);
#endif  // SB_API_VERSION >= 15
  if (destructor) {
    destructor(event.data);
  }
}

void DeleteStartData(void* data) {
  SbEventStartData* start_data = static_cast<SbEventStartData*>(data);
  if (start_data) {
    delete[] start_data->argument_values;
  }
  delete start_data;
}

}  // namespace

// The next event ID to use for Schedule().
volatile SbAtomic32 g_next_event_id = 0;

Application* Application::g_instance = NULL;

#if SB_API_VERSION >= 15
Application::Application(SbEventHandleCallback sb_event_handle_callback)
    : error_level_(0),
      thread_(SbThreadGetCurrent()),
      start_link_(NULL),
      state_(kStateUnstarted),
      sb_event_handle_callback_(sb_event_handle_callback) {
  SB_CHECK(sb_event_handle_callback_)
      << "sb_event_handle_callback_ has not been set.";
#else
Application::Application()
    : error_level_(0),
      thread_(SbThreadGetCurrent()),
      start_link_(NULL),
      state_(kStateUnstarted) {
#endif  // SB_API_VERSION >= 15
  Application* old_instance =
      reinterpret_cast<Application*>(SbAtomicAcquire_CompareAndSwapPtr(
          reinterpret_cast<SbAtomicPtr*>(&g_instance),
          reinterpret_cast<SbAtomicPtr>(reinterpret_cast<void*>(NULL)),
          reinterpret_cast<SbAtomicPtr>(this)));
  SB_DCHECK(!old_instance);
}

Application::~Application() {
  Application* old_instance =
      reinterpret_cast<Application*>(SbAtomicAcquire_CompareAndSwapPtr(
          reinterpret_cast<SbAtomicPtr*>(&g_instance),
          reinterpret_cast<SbAtomicPtr>(this),
          reinterpret_cast<SbAtomicPtr>(reinterpret_cast<void*>(NULL))));
  SB_DCHECK(old_instance);
  SB_DCHECK(old_instance == this);
  SbMemoryDeallocate(start_link_);
}

int Application::Run(CommandLine command_line, const char* link_data) {
  Initialize();
  command_line_.reset(new CommandLine(command_line));
  if (link_data) {
    SetStartLink(link_data);
  }

  return RunLoop();
}

int Application::Run(CommandLine command_line) {
  Initialize();
  command_line_.reset(new CommandLine(command_line));

  if (command_line_->HasSwitch(kLinkSwitch)) {
    std::string value = command_line_->GetSwitchValue(kLinkSwitch);
    if (!value.empty()) {
      SetStartLink(value.c_str());
    }
  }

  if (command_line_->HasSwitch(kMinLogLevel)) {
    ::starboard::logging::SetMinLogLevel(::starboard::logging::StringToLogLevel(
        command_line_->GetSwitchValue(kMinLogLevel)));
  } else {
#if SB_LOGGING_IS_OFFICIAL_BUILD
    ::starboard::logging::SetMinLogLevel(::starboard::logging::SB_LOG_FATAL);
#else
    ::starboard::logging::SetMinLogLevel(::starboard::logging::SB_LOG_INFO);
#endif
  }

  return RunLoop();
}

const CommandLine* Application::GetCommandLine() {
  return command_line_.get();
}

void Application::Blur(void* context, EventHandledCallback callback) {
  Inject(new Event(kSbEventTypeBlur, context, callback));
}

void Application::Focus(void* context, EventHandledCallback callback) {
  Inject(new Event(kSbEventTypeFocus, context, callback));
}

void Application::Conceal(void* context, EventHandledCallback callback) {
  Inject(new Event(kSbEventTypeConceal, context, callback));
}

void Application::Reveal(void* context, EventHandledCallback callback) {
  Inject(new Event(kSbEventTypeReveal, context, callback));
}

void Application::Freeze(void* context, EventHandledCallback callback) {
  Inject(new Event(kSbEventTypeFreeze, context, callback));
}

void Application::Unfreeze(void* context, EventHandledCallback callback) {
  Inject(new Event(kSbEventTypeUnfreeze, context, callback));
}

void Application::Stop(int error_level) {
  Event* event = new Event(kSbEventTypeStop, NULL, NULL);
  event->error_level = error_level;
  Inject(event);
}

void Application::Link(const char* link_data) {
  SB_DCHECK(link_data) << "You must call Link with link_data.";
  Inject(new Event(kSbEventTypeLink, SbStringDuplicate(link_data),
                   SbMemoryDeallocate));
}

void Application::InjectLowMemoryEvent() {
  Inject(new Event(kSbEventTypeLowMemory, NULL, NULL));
}

void Application::InjectOsNetworkDisconnectedEvent() {
  Inject(new Event(kSbEventTypeOsNetworkDisconnected, NULL, NULL));
}

void Application::InjectOsNetworkConnectedEvent() {
  Inject(new Event(kSbEventTypeOsNetworkConnected, NULL, NULL));
}

void Application::WindowSizeChanged(void* context,
                                    EventHandledCallback callback) {
  Inject(new Event(kSbEventTypeWindowSizeChanged, context, callback));
}

SbEventId Application::Schedule(SbEventCallback callback,
                                void* context,
                                SbTimeMonotonic delay) {
  SbEventId id = SbAtomicNoBarrier_Increment(&g_next_event_id, 1);
  InjectTimedEvent(new TimedEvent(id, callback, context, delay));
  return id;
}

void Application::Cancel(SbEventId id) {
  CancelTimedEvent(id);
}

void Application::HandleFrame(SbPlayer player,
                              const scoped_refptr<VideoFrame>& frame,
                              int z_index,
                              int x,
                              int y,
                              int width,
                              int height) {
  AcceptFrame(player, frame, z_index, x, y, width, height);
}

void Application::SetStartLink(const char* start_link) {
  SB_DCHECK(IsCurrentThread());
  SbMemoryDeallocate(start_link_);
  if (start_link) {
    start_link_ = SbStringDuplicate(start_link);
  } else {
    start_link_ = NULL;
  }
}

void Application::DispatchStart(SbTimeMonotonic timestamp) {
  SB_DCHECK(IsCurrentThread());
  SB_DCHECK(state_ == kStateUnstarted);
  DispatchAndDelete(CreateInitialEvent(kSbEventTypeStart, timestamp));
}

void Application::DispatchPreload(SbTimeMonotonic timestamp) {
  SB_DCHECK(IsCurrentThread());
  SB_DCHECK(state_ == kStateUnstarted);
  DispatchAndDelete(CreateInitialEvent(kSbEventTypePreload, timestamp));
}

bool Application::HasPreloadSwitch() {
  return command_line_->HasSwitch(kPreloadSwitch);
}

bool Application::DispatchAndDelete(Application::Event* event) {
  SB_DCHECK(IsCurrentThread());
  if (!event) {
    return true;
  }

  // Ensure the event is deleted unless it is released.
  scoped_ptr<Event> scoped_event(event);

  // Ensure that we go through the the appropriate lifecycle events based on
  // the current state. If intermediate events need to be processed, use
  // HandleEventAndUpdateState() rather than Inject() for the intermediate
  // events because there may already be other lifecycle events in the queue.

  SbTimeMonotonic timestamp = scoped_event->event->timestamp;
  switch (scoped_event->event->type) {
    case kSbEventTypePreload:
      if (state() != kStateUnstarted) {
        return true;
      }
      break;
    case kSbEventTypeStart:
      if (state() != kStateUnstarted && state() != kStateStarted) {
        HandleEventAndUpdateState(
            new Event(kSbEventTypeFocus, timestamp, NULL, NULL));
        return true;
      }
      break;
    case kSbEventTypeBlur:
      if (state() != kStateStarted) {
        return true;
      }
      break;
    case kSbEventTypeFocus:
      switch (state()) {
        case kStateStopped:
          return true;
        case kStateFrozen:
          HandleEventAndUpdateState(
              new Event(kSbEventTypeUnfreeze, timestamp, NULL, NULL));
        // The fall-through is intentional.
        case kStateConcealed:
          HandleEventAndUpdateState(
              new Event(kSbEventTypeReveal, timestamp, NULL, NULL));
          HandleEventAndUpdateState(scoped_event.release());
          return true;
        case kStateBlurred:
          break;
        case kStateStarted:
        case kStateUnstarted:
          return true;
      }
      break;
    case kSbEventTypeConceal:
      switch (state()) {
        case kStateUnstarted:
          return true;
        case kStateStarted:
          HandleEventAndUpdateState(
              new Event(kSbEventTypeBlur, timestamp, NULL, NULL));
          HandleEventAndUpdateState(scoped_event.release());
          return true;
        case kStateBlurred:
          break;
        case kStateConcealed:
        case kStateFrozen:
        case kStateStopped:
          return true;
      }
      break;
    case kSbEventTypeReveal:
      switch (state()) {
        case kStateStopped:
          return true;
        case kStateFrozen:
          HandleEventAndUpdateState(
              new Event(kSbEventTypeUnfreeze, timestamp, NULL, NULL));
          HandleEventAndUpdateState(scoped_event.release());
          return true;
        case kStateConcealed:
          break;
        case kStateBlurred:
        case kStateStarted:
        case kStateUnstarted:
          return true;
      }
      break;
    case kSbEventTypeFreeze:
      switch (state()) {
        case kStateUnstarted:
          return true;
        case kStateStarted:
          HandleEventAndUpdateState(
              new Event(kSbEventTypeBlur, timestamp, NULL, NULL));
        // The fall-through is intentional
        case kStateBlurred:
          HandleEventAndUpdateState(
              new Event(kSbEventTypeConceal, timestamp, NULL, NULL));
          HandleEventAndUpdateState(scoped_event.release());
          return true;
        case kStateConcealed:
          break;
        case kStateFrozen:
        case kStateStopped:
          return true;
      }
      break;
    case kSbEventTypeUnfreeze:
      switch (state()) {
        case kStateStopped:
          return true;
        case kStateFrozen:
          break;
        case kStateConcealed:
        case kStateBlurred:
        case kStateStarted:
        case kStateUnstarted:
          return true;
      }
      break;
    case kSbEventTypeStop:
      switch (state()) {
        case kStateUnstarted:
          return true;
        case kStateStarted:
          HandleEventAndUpdateState(
              new Event(kSbEventTypeBlur, timestamp, NULL, NULL));
        // The fall-through is intentional.
        case kStateBlurred:
          HandleEventAndUpdateState(
              new Event(kSbEventTypeConceal, timestamp, NULL, NULL));
        // The fall-through is intentional.
        case kStateConcealed:
          HandleEventAndUpdateState(
              new Event(kSbEventTypeFreeze, timestamp, NULL, NULL));
          // There is a race condition with kSbEventTypeStop processing and
          // timed events currently in use. Processing the intermediate events
          // takes time, so makes it more likely that a timed event will be due
          // immediately and processed immediately afterward. The event(s) need
          // to be fixed to behave better after kSbEventTypeStop has been
          // handled. In the meantime, continue to use Inject() to preserve the
          // current timing. This bug can still happen with Inject(), but it is
          // less likely than if HandleEventAndUpdateState() were used.
          Inject(scoped_event.release());
          return true;
        case kStateFrozen:
          break;
        case kStateStopped:
          return true;
      }
      error_level_ = scoped_event->error_level;
      break;
    case kSbEventTypeScheduled: {
      TimedEvent* timed_event =
          reinterpret_cast<TimedEvent*>(scoped_event->event->data);
      timed_event->callback(timed_event->context);
      return true;
    }
    default:
      break;
  }

  return HandleEventAndUpdateState(scoped_event.release());
}

bool Application::HandleEventAndUpdateState(Application::Event* event) {
  // Ensure the event is deleted unless it is released.
  scoped_ptr<Event> scoped_event(event);

  // Call OnSuspend() and OnResume() before the event as needed.
  if (scoped_event->event->type == kSbEventTypeUnfreeze &&
      state() == kStateFrozen) {
    OnResume();
  } else if (scoped_event->event->type == kSbEventTypeFreeze &&
             state() == kStateConcealed) {
    OnSuspend();
  }

#if SB_API_VERSION >= 15
  sb_event_handle_callback_(scoped_event->event);
#else
  SbEventHandle(scoped_event->event);
#endif  // SB_API_VERSION >= 15

  switch (scoped_event->event->type) {
    case kSbEventTypePreload:
      SB_DCHECK(state() == kStateUnstarted);
      state_ = kStateConcealed;
      break;
    case kSbEventTypeStart:
      SB_DCHECK(state() == kStateUnstarted);
      state_ = kStateStarted;
      break;
    case kSbEventTypeBlur:
      SB_DCHECK(state() == kStateStarted);
      state_ = kStateBlurred;
      break;
    case kSbEventTypeFocus:
      SB_DCHECK(state() == kStateBlurred);
      state_ = kStateStarted;
      break;
    case kSbEventTypeConceal:
      SB_DCHECK(state() == kStateBlurred);
      state_ = kStateConcealed;
      break;
    case kSbEventTypeReveal:
      SB_DCHECK(state() == kStateConcealed);
      state_ = kStateBlurred;
      break;
    case kSbEventTypeFreeze:
      SB_DCHECK(state() == kStateConcealed);
      state_ = kStateFrozen;
      break;
    case kSbEventTypeUnfreeze:
      SB_DCHECK(state() == kStateFrozen);
      state_ = kStateConcealed;
      break;
    case kSbEventTypeStop:
      SB_DCHECK(state() == kStateFrozen);
      state_ = kStateStopped;
      return false;
    default:
      break;
  }
  // Should not be unstarted after the first event.
  SB_DCHECK(state() != kStateUnstarted);
  return true;
}

void Application::CallTeardownCallbacks() {
  ScopedLock lock(callbacks_lock_);
  for (size_t i = 0; i < teardown_callbacks_.size(); ++i) {
    teardown_callbacks_[i]();
  }
}

Application::Event* Application::CreateInitialEvent(SbEventType type,
                                                    SbTimeMonotonic timestamp) {
  SB_DCHECK(type == kSbEventTypePreload || type == kSbEventTypeStart);
  SbEventStartData* start_data = new SbEventStartData();
  memset(start_data, 0, sizeof(SbEventStartData));
  const CommandLine::StringVector& args = command_line_->argv();
  start_data->argument_count = static_cast<int>(args.size());
  // Cobalt web_platform_tests expect an extra argv[argc] set to NULL.
  start_data->argument_values = new char*[start_data->argument_count + 1];
  start_data->argument_values[start_data->argument_count] = NULL;
  for (int i = 0; i < start_data->argument_count; i++) {
    start_data->argument_values[i] = const_cast<char*>(args[i].c_str());
  }
  start_data->link = start_link_;

  return new Event(type, timestamp, start_data, &DeleteStartData);
}

int Application::RunLoop() {
  SB_DCHECK(command_line_);
  if (IsPreloadImmediate()) {
    DispatchPreload(SbTimeGetMonotonicNow());
  } else if (IsStartImmediate()) {
    DispatchStart(SbTimeGetMonotonicNow());
  }

  for (;;) {
    if (!DispatchNextEvent()) {
      break;
    }
  }

  CallTeardownCallbacks();
  Teardown();
  return error_level_;
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
