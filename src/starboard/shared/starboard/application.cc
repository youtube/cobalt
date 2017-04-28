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

#include "starboard/atomic.h"
#include "starboard/condition_variable.h"
#include "starboard/event.h"
#include "starboard/memory.h"
#include "starboard/string.h"

#include "starboard/shared/starboard/command_line.h"

namespace starboard {
namespace shared {
namespace starboard {

namespace {

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

int Application::Run(int argc, char** argv) {
  Initialize();
  command_line_.reset(new CommandLine(argc, argv));
  if (IsStartImmediate()) {
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

CommandLine* Application::GetCommandLine() {
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

#if SB_HAS(PLAYER) && (SB_API_VERSION >= 4 || SB_IS(PLAYER_PUNCHED_OUT))

void Application::HandleFrame(SbPlayer player,
                              const scoped_refptr<VideoFrame>& frame,
                              int x,
                              int y,
                              int width,
                              int height) {
  AcceptFrame(player, frame, x, y, width, height);
}
#endif  // SB_HAS(PLAYER) && (SB_API_VERSION >= 4 || SB_IS(PLAYER_PUNCHED_OUT))

void Application::SetStartLink(const char* start_link) {
  SbMemoryDeallocate(start_link_);
  if (start_link) {
    start_link_ = SbStringDuplicate(start_link);
  } else {
    start_link_ = NULL;
  }
}

void Application::DispatchStart() {
  SB_DCHECK(state_ == kStateUnstarted);
  SbEventStartData start_data;
  start_data.argument_values =
      const_cast<char**>(command_line_->GetOriginalArgv());
  start_data.argument_count = command_line_->GetOriginalArgc();
  start_data.link = start_link_;
  Dispatch(kSbEventTypeStart, &start_data, NULL);
  state_ = kStateStarted;
}

bool Application::DispatchAndDelete(Application::Event* event) {
  if (!event) {
    return true;
  }

  // DispatchStart() must be called first
  SB_DCHECK(state_ != kStateUnstarted);

  // Ensure that we go through the the appropriate lifecycle events based on the
  // current state.
  switch (event->event->type) {
    case kSbEventTypePause:
      if (state() != kStateStarted) {
        delete event;
        return true;
      }
      break;
    case kSbEventTypeUnpause:
      if (state() == kStateStarted) {
        delete event;
        return true;
      }

      if (state() == kStateSuspended) {
        Inject(new Event(kSbEventTypeResume, NULL, NULL));
        Inject(event);
        return true;
      }
      break;
    case kSbEventTypeSuspend:
      if (state() == kStateSuspended) {
        delete event;
        return true;
      }

      if (state() == kStateStarted) {
        Inject(new Event(kSbEventTypePause, NULL, NULL));
        Inject(event);
        return true;
      }
      break;
    case kSbEventTypeResume:
      if (state() == kStateStarted || state() == kStatePaused) {
        delete event;
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
        Inject(event);
        return true;
      }

      if (state() == kStatePaused) {
        Inject(new Event(kSbEventTypeSuspend, NULL, NULL));
        Inject(event);
        return true;
      }
      error_level_ = event->error_level;
      break;
    case kSbEventTypeScheduled: {
      TimedEvent* timed_event =
          reinterpret_cast<TimedEvent*>(event->event->data);
      timed_event->callback(timed_event->context);
      delete event;
      return true;
    }
    default:
      break;
  }

  SbEventHandle(event->event);

  bool should_continue = true;
  switch (event->event->type) {
    case kSbEventTypePause:
      SB_DCHECK(state() == kStateStarted);
      state_ = kStatePaused;
      break;
    case kSbEventTypeUnpause:
      SB_DCHECK(state() == kStatePaused);
      state_ = kStateStarted;
      break;
    case kSbEventTypeSuspend:
      SB_DCHECK(state() == kStatePaused);
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
      should_continue = false;
      break;
    default:
      break;
  }

  delete event;
  return should_continue;
}

void Application::CallTeardownCallbacks() {
  ScopedLock lock(callbacks_lock_);
  for (size_t i = 0; i < teardown_callbacks_.size(); ++i) {
    teardown_callbacks_[i]();
  }
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
