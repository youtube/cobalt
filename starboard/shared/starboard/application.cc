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

#include "starboard/shared/starboard/application.h"

#include <string>

#include "starboard/atomic.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/condition_variable.h"
#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/string.h"

#include "starboard/shared/starboard/command_line.h"

namespace starboard {
namespace shared {
namespace starboard {

namespace {

const char kPreloadSwitch[] = "preload";
const char kLinkSwitch[] = "link";

// Dispatches an event of |type| with |data| to the system event handler,
// calling |destructor| on |data| when finished dispatching. Does all
// appropriate NULL checks so you don't have to.
void Dispatch(SbEventType type, void* data, SbEventDataDestructor destructor) {
  SbEvent event;
  event.type = type;
  event.data = data;
  SbEventHandle(&event);
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

Application::Application()
    : error_level_(0),
      thread_(SbThreadGetCurrent()),
      start_link_(NULL),
      state_(kStateUnstarted) {
  Application* old_instance =
      reinterpret_cast<Application*>(SbAtomicAcquire_CompareAndSwapPtr(
          reinterpret_cast<SbAtomicPtr*>(&g_instance),
          reinterpret_cast<SbAtomicPtr>(reinterpret_cast<void*>(NULL)),
          reinterpret_cast<SbAtomicPtr>(this)));
  SB_DCHECK(!old_instance);
  SB_UNREFERENCED_PARAMETER(old_instance);
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

  return RunLoop();
}

const CommandLine* Application::GetCommandLine() {
  return command_line_.get();
}

void Application::Pause(void* context, EventHandledCallback callback) {
  Inject(new Event(kSbEventTypePause, context, callback));
}

void Application::Unpause(void* context, EventHandledCallback callback) {
  Inject(new Event(kSbEventTypeUnpause, context, callback));
}

void Application::Suspend(void* context, EventHandledCallback callback) {
  Inject(new Event(kSbEventTypeSuspend, context, callback));
}

void Application::Resume(void* context, EventHandledCallback callback) {
  Inject(new Event(kSbEventTypeResume, context, callback));
}

void Application::Stop(int error_level) {
  Event* event = new Event(kSbEventTypeStop, NULL, NULL);
  event->error_level = error_level;
  Inject(event);
}

void Application::Link(const char *link_data) {
  SB_DCHECK(link_data) << "You must call Link with link_data.";
  Inject(new Event(kSbEventTypeLink, SbStringDuplicate(link_data),
                   SbMemoryDeallocate));
}

void Application::InjectLowMemoryEvent() {
#if SB_API_VERSION >= 6
  Inject(new Event(kSbEventTypeLowMemory, NULL, NULL));
#endif  // SB_API_VERSION >= 6
}

#if SB_API_VERSION >= SB_WINDOW_SIZE_CHANGED_API_VERSION
void Application::WindowSizeChanged(void* context,
                                    EventHandledCallback callback) {
  Inject(new Event(kSbEventTypeWindowSizeChanged, context, callback));
}
#endif  // SB_API_VERSION >= SB_WINDOW_SIZE_CHANGED_API_VERSION

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

void Application::DispatchStart() {
  SB_DCHECK(IsCurrentThread());
  SB_DCHECK(state_ == kStateUnstarted || state_ == kStatePreloading);
  DispatchAndDelete(CreateInitialEvent(kSbEventTypeStart));
}

void Application::DispatchPreload() {
  SB_DCHECK(IsCurrentThread());
#if SB_API_VERSION >= 6
  SB_DCHECK(state_ == kStateUnstarted);
  DispatchAndDelete(CreateInitialEvent(kSbEventTypePreload));
#else  // SB_API_VERSION >= 6
  SB_NOTREACHED();
#endif  // SB_API_VERSION >= 6
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

  // Ensure that we go through the the appropriate lifecycle events based on the
  // current state.
  switch (scoped_event->event->type) {
#if SB_API_VERSION >= 6
    case kSbEventTypePreload:
      if (state() != kStateUnstarted) {
        return true;
      }
      break;
#endif  // SB_API_VERSION >= 6
    case kSbEventTypeStart:
      if (state() != kStatePreloading && state() != kStateUnstarted) {
        return true;
      }
      break;
    case kSbEventTypePause:
      if (state() != kStateStarted) {
        return true;
      }
      break;
    case kSbEventTypeUnpause:
      if (state() == kStateStarted) {
        return true;
      }

      if (state() == kStatePreloading) {
        // Convert to Start event and consume.
        DispatchStart();
        return true;
      }

      if (state() == kStateSuspended) {
        Inject(new Event(kSbEventTypeResume, NULL, NULL));
        Inject(scoped_event.release());
        return true;
      }
      break;
    case kSbEventTypeSuspend:
      if (state() == kStateSuspended) {
        return true;
      }

      if (state() == kStatePreloading) {
        // If Preloading, we can jump straight to Suspended, so we don't try to
        // do anything fancy here.
        break;
      }

      if (state() == kStateStarted) {
        Inject(new Event(kSbEventTypePause, NULL, NULL));
        Inject(scoped_event.release());
        return true;
      }
      break;
    case kSbEventTypeResume:
      if (state() == kStateStarted || state() == kStatePaused) {
        return true;
      }
      if (state() == kStateSuspended) {
        OnResume();
      }
      break;
    case kSbEventTypeStop:
      if (state() == kStateStarted) {
        Inject(new Event(kSbEventTypePause, NULL, NULL));
        Inject(new Event(kSbEventTypeSuspend, NULL, NULL));
        Inject(scoped_event.release());
        return true;
      }

      if (state() == kStatePaused || state() == kStatePreloading) {
        Inject(new Event(kSbEventTypeSuspend, NULL, NULL));
        Inject(scoped_event.release());
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

  SbEventHandle(scoped_event->event);

  switch (scoped_event->event->type) {
#if SB_API_VERSION >= 6
    case kSbEventTypePreload:
      SB_DCHECK(state() == kStateUnstarted);
      state_ = kStatePreloading;
      break;
#endif  // SB_API_VERSION >= 6
    case kSbEventTypeStart:
      SB_DCHECK(state() == kStatePreloading || state() == kStateUnstarted);
      state_ = kStateStarted;
      break;
    case kSbEventTypePause:
      SB_DCHECK(state() == kStateStarted);
      state_ = kStatePaused;
      break;
    case kSbEventTypeUnpause:
      SB_DCHECK(state() == kStatePaused);
      state_ = kStateStarted;
      break;
    case kSbEventTypeSuspend:
      SB_DCHECK(state() == kStatePreloading || state() == kStatePaused);
      state_ = kStateSuspended;
      OnSuspend();
      break;
    case kSbEventTypeResume:
      SB_DCHECK(state() == kStateSuspended);
      state_ = kStatePaused;
      break;
    case kSbEventTypeStop:
      SB_DCHECK(state() == kStateSuspended);
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

Application::Event* Application::CreateInitialEvent(SbEventType type) {
#if SB_API_VERSION >= 6
  SB_DCHECK(type == kSbEventTypePreload || type == kSbEventTypeStart);
#else  // SB_API_VERSION >= 6
  SB_DCHECK(type == kSbEventTypeStart);
#endif  // SB_API_VERSION >= 6
  SbEventStartData* start_data = new SbEventStartData();
  SbMemorySet(start_data, 0, sizeof(SbEventStartData));
  const CommandLine::StringVector& args = command_line_->argv();
  start_data->argument_count = static_cast<int>(args.size());
  // Cobalt web_platform_tests expect an extra argv[argc] set to NULL.
  start_data->argument_values = new char*[start_data->argument_count + 1];
  start_data->argument_values[start_data->argument_count] = NULL;
  for (int i=0; i < start_data->argument_count; i++) {
    start_data->argument_values[i] = const_cast<char*>(args[i].c_str());
  }
  start_data->link = start_link_;
  return new Event(type, start_data, &DeleteStartData);
}

int Application::RunLoop() {
  SB_DCHECK(command_line_);
  if (IsPreloadImmediate()) {
    DispatchPreload();
  } else if (IsStartImmediate()) {
    DispatchStart();
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
