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
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {

// A small application framework for managing the application life-cycle, and
// dispatching events to the Starboard event handler, SbEventHandle.
class Application {
 public:
  // Structure to keep track of scheduled events, also used as the data argument
  // for kSbEventTypeScheduled Events.
  struct TimedEvent {
    TimedEvent(SbEventId eid,
               SbEventCallback func,
               void* data,
               SbTimeMonotonic delay)
        : id(eid),
          callback(func),
          context(data),
          target_time(delay + SbTimeGetMonotonicNow()),
          canceled(false) {}

    SbEventId id;
    SbEventCallback callback;
    void* context;
    SbTimeMonotonic target_time;
    bool canceled;
  };

  // Destructor function that deletes the value as the parameterized type.
  template <typename T>
  static void DeleteDestructor(void* value) {
    delete static_cast<T*>(value);
  }

  // Destructor function that deletes the value as an array of the
  // parameterized type.
  template <typename T>
  static void DeleteArrayDestructor(void* value) {
    delete[] static_cast<T*>(value);
  }

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
    explicit Event(TimedEvent* data)
        : event(new SbEvent()),
          destructor(&DeleteDestructor<TimedEvent>),
          error_level(0) {
      event->type = kSbEventTypeScheduled;
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
  // possible. May be called from an external thread.
  void Stop(int error_level);

  // Schedules an event into the event queue.  May be called from an external
  // thread.
  SbEventId Schedule(SbEventCallback callback,
                     void* context,
                     SbTimeMonotonic delay);

  // Cancels an event that was previously scheduled.  May be called from an
  // external thread.
  void Cancel(SbEventId id);

  // Handles receiving a new video frame from the media system. Only used when
  // the application needs to composite video frames with punch-out video
  // manually (should be rare). Will be called from an external thread.
  void HandleFrame(const player::VideoFrame& frame);

 protected:
  // Initializes any systems that need initialization before application
  // start. Subclasses may override this method to run initialization code that
  // must be run before application start event is handled.
  virtual void Initialize() {}

  // Tears down any systems that need tearing down before application
  // termination. Subclasses may override this method to run teardown code that
  // must be run after the application stop event is handled.
  virtual void Teardown() {}

  // Subclasses may override this method to accept video frames from the media
  // system. Will be called from an external thread.
  virtual void AcceptFrame(const player::VideoFrame& frame) {}

  // Blocks until the next event is available. Subclasses must implement this
  // method to provide events for the platform. Gives ownership to the caller.
  virtual Event* GetNextEvent() = 0;

  // Injects an event into the queue, such that it will be returned from
  // GetNextEvent(), giving ownership of the event. NULL is valid, and will just
  // wake up the main loop. May be called from an external thread. Subclasses
  // must implement this method.
  virtual void Inject(Event* event) = 0;

  // Injects a new TimedEvent into the scheduled event queue, passing
  // ownership. May be called from an external thread.
  virtual void InjectTimedEvent(TimedEvent* timed_event) = 0;

  // Cancels the timed event associated with the given SbEventId, if it hasn't
  // already fired. May be called from an external thread.
  virtual void CancelTimedEvent(SbEventId event_id) = 0;

  // Gets the next timed event that has met or passed its target time. Returns
  // NULL if there are no due TimedEvents queued. Passes ownership to caller.
  virtual TimedEvent* GetNextDueTimedEvent() = 0;

  // Gets the next time that a TimedEvent is due. Returns
  // SbTimeGetMonotonicNow() if the next TimedEvent is past due. Returns
  // kSbTimeMax if there are no queued TimedEvents.
  virtual SbTimeMonotonic GetNextTimedEventTargetTime() = 0;

  // Sets the launch deep link string, if any, which is passed in the start
  // event that initializes and starts Cobalt.
  void SetStartLink(const char* start_link);

  bool IsCurrentThread() const {
    return SbThreadIsEqual(thread_, SbThreadGetCurrent());
  }

 private:
  // Dispatches |event| to the system event handler, taking ownership of the
  // event. Returns whether to keep servicing the event queue, i.e. false means
  // to abort the event queue.
  bool DispatchAndDelete(Application::Event* event);

  // The single application instance.
  static Application* g_instance;

  // The error_level set by the last call to Stop().
  int error_level_;

  // The thread that this application was created on, which is assumed to be the
  // main thread.
  SbThread thread_;

  // The deep link included in the Start event sent to Cobalt. Initially NULL,
  // derived classes may set it during initialization using |SetStartLink|.
  char* start_link_;
};

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_APPLICATION_H_
