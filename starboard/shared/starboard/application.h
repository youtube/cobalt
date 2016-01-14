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

// A cross-platform base application engine that is used to manage the main
// event loop

#ifndef STARBOARD_SHARED_STARBOARD_APPLICATION_H_
#define STARBOARD_SHARED_STARBOARD_APPLICATION_H_

#include "starboard/atomic.h"
#include "starboard/condition_variable.h"
#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/shared/internal_only.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {

// A small application framework for managing the application life-cycle, and
// dispatching events to the Starboard event handler, SbEventHandle.
class Application {
 public:
  Application();
  virtual ~Application();

  // Gets the current instance of the Application. DCHECKS if called before the
  // application has been constructed.
  static inline Application* Get() {
    Application* instance = reinterpret_cast<Application*>(
        SbAtomicAcquire_LoadPtr(reinterpret_cast<SbAtomicPtr*>(&g_instance)));
    SB_DCHECK(instance);
    return instance;
  }

  // Runs the application with the current thread as the Main Starboard Thread,
  // blocking until application exit. This method will dispatch all appropriate
  // initialization and teardown events. Returns the resulting error level.
  int Run(int argc, char** argv);

  // Signals that the application should gracefully terminate as soon as
  // possible. May be called from any thread.
  void Stop(int error_level);

  // Injects a kSbEventTypeApplication event into the event queue.
  void InjectApplicationEvent(void* data, SbEventDataDestructor destructor);

 protected:
  // A Starboard event and its destructor. Takes ownership of the event, thus
  // deleting the event and calling the destructor on its data when it is
  // deleted.
  struct Event {
    Event(SbEvent* event, SbEventDataDestructor destructor)
        : event(event), destructor(destructor), error_level(0) {}
    Event(SbEventType type, void* data, SbEventDataDestructor destructor)
        : event(new SbEvent()), destructor(destructor), error_level(0) {
      event->type = type;
      event->data = data;
    }

    ~Event() {
      if (destructor && event->data) {
        destructor(event->data);
      }
      if (event) {
        delete event;
      }
    }

    SbEvent* event;
    SbEventDataDestructor destructor;
    int error_level;
  };

  // Initializes any systems that need initialization before application
  // start. Subclasses may override this method to run initialization code that
  // must be run before application start event is handled.
  virtual void Initialize() {}

  // Tears down any systems that need tearing down before application
  // termination. Subclasses may override this method to run teardown code that
  // must be run after the application stop event is handled.
  virtual void Teardown() {}

  // Blocks until the next event is available. Subclasses must implement this
  // method to provide events for the platform. Gives ownership to the caller.
  virtual Event* GetNextEvent() = 0;

  // Injects an event into the queue, such that it will be returned from
  // GetNextEvent(), giving ownership of the event. NULL is valid, and will just
  // wake up the main loop. Subclasses must implement this method.
  virtual void Inject(Event* event) = 0;

 private:
  // Dispatches |event| to the system event handler, taking ownership of the
  // event. Returns whether to keep servicing the event queue, i.e. false means
  // to abort the event queue.
  bool DispatchAndDelete(Application::Event* event);

  // The single application instance.
  static Application* g_instance;

  // The error_level set by the last call to Stop().
  int error_level_;

  // A flag that can be set to cause the application to shut down.
  bool should_stop_;

  // The queue of events that have not yet been dispatched.
  Queue<Event*> event_queue_;
};

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_APPLICATION_H_
