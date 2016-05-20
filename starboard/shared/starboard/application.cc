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
#include "starboard/log.h"

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
  if (destructor && event.data) {
    destructor(event.data);
  }
}

// Dispatches a Start event to the system event handler.
void DispatchStart(int argc, char** argv) {
  SbEventStartData start_data;
  start_data.argument_values = argv;
  start_data.argument_count = argc;
  start_data.link_data.link_data = NULL;
  start_data.link_data.link_data_size = 0;
  Dispatch(kSbEventTypeStart, &start_data, NULL);
}

// The next event ID to use for Schedule().
volatile SbAtomic32 g_next_event_id = 0;

}  // namespace

Application* Application::g_instance = NULL;

Application::Application() : error_level_(0), thread_(SbThreadGetCurrent()) {
  Application* old_instance =
      reinterpret_cast<Application*>(SbAtomicAcquire_CompareAndSwapPtr(
          reinterpret_cast<SbAtomicPtr*>(&g_instance),
          reinterpret_cast<SbAtomicPtr>(NULL),
          reinterpret_cast<SbAtomicPtr>(this)));
  SB_DCHECK(!old_instance);
}

Application::~Application() {
  Application* old_instance =
      reinterpret_cast<Application*>(SbAtomicAcquire_CompareAndSwapPtr(
          reinterpret_cast<SbAtomicPtr*>(&g_instance),
          reinterpret_cast<SbAtomicPtr>(this),
          reinterpret_cast<SbAtomicPtr>(NULL)));
  SB_DCHECK(old_instance);
  SB_DCHECK(old_instance == this);
}

int Application::Run(int argc, char** argv) {
  Initialize();
  DispatchStart(argc, argv);

  for (;;) {
    if (!DispatchAndDelete(GetNextEvent())) {
      break;
    }
  }

  Teardown();
  return error_level_;
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

void Application::HandleFrame(const VideoFrame& frame) {
  AcceptFrame(frame);
}

bool Application::DispatchAndDelete(Application::Event* event) {
  if (!event) {
    return true;
  }

  bool should_continue = true;
  if (event->event->type == kSbEventTypeStop) {
    should_continue = false;
    error_level_ = event->error_level;
  }

  if (event->event->type == kSbEventTypeScheduled) {
    TimedEvent* timed_event = reinterpret_cast<TimedEvent*>(event->event->data);
    timed_event->callback(timed_event->context);
  } else {
    SbEventHandle(event->event);
  }
  delete event;
  return should_continue;
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
