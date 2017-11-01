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

#include <vector>

#include "starboard/atomic.h"
#include "starboard/condition_variable.h"
#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "starboard/types.h"
#include "starboard/window.h"

namespace starboard {
namespace shared {
namespace starboard {

// A small application framework for managing the application life-cycle, and
// dispatching events to the Starboard event handler, SbEventHandle.
class Application {
 public:
  typedef player::VideoFrame VideoFrame;

  // You can use a void(void *) function to signal that a state-transition event
  // has completed.
  typedef SbEventDataDestructor EventHandledCallback;

  // Signature for a function that will be called at the beginning of Teardown.
  typedef void (*TeardownCallback)(void);

  // Enumeration of states that the application can be in.
  enum State {
    // The initial Unstarted state.
    kStateUnstarted,

    // The preloading state, where the application gets as much work done as
    // possible to launch, but is not visible. You see exits to kStateStarted
    // and kStateSuspended.
    kStatePreloading,

    // The normal foreground, fully-visible state after receiving the initial
    // START event or after UNPAUSE from Paused.
    kStateStarted,

    // The background-but-visible or partially-obscured state after receiving an
    // PAUSE event from Started or RESUME event from Suspended.
    kStatePaused,

    // The fully-obscured or about-to-be-terminated state after receiving a
    // SUSPEND event in Paused.
    kStateSuspended,

    // The completely terminated state after receiving the STOP event in the
    // Suspended state.
    kStateStopped,
  };

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
      if (destructor) {
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
  int Run(int argc, char** argv, const char* link_data);
  int Run(int argc, char** argv);

  // Retrieves the CommandLine for the application.
  // NULL until Run() is called.
  CommandLine* GetCommandLine();

  // Signals that the application should transition from STARTED to PAUSED as
  // soon as possible. Does nothing if already PAUSED or SUSPENDED. May be
  // called from an external thread.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void Pause(void* context, EventHandledCallback callback);

  // Signals that the application should transition to STARTED as soon as
  // possible, moving through all required state transitions to get there. Does
  // nothing if already STARTED. May be called from an external thread.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void Unpause(void* context, EventHandledCallback callback);

  // Signals that the application should transition to SUSPENDED as soon as
  // possible, moving through all required state transitions to get there. Does
  // nothing if already SUSPENDED. May be called from an external thread.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void Suspend(void* context, EventHandledCallback callback);

  // Signals that the application should transition to PAUSED from SUSPENDED as
  // soon as possible. Does nothing if already PAUSED or STARTED. May be called
  // from an external thread.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void Resume(void* context, EventHandledCallback callback);

  // Signals that the application should gracefully terminate as soon as
  // possible. Will transition through PAUSED and SUSPENDED to STOPPED as
  // appropriate for the current state. May be called from an external thread.
  void Stop(int error_level);

  // Injects a link event to the application with the given |link_data|, which
  // must be a null-terminated string. Makes a copy of |link_data|, so it only
  // has to live over the lifetime of the call to Link. May be called from an
  // external thread.
  void Link(const char* link_data);

  // Injects an event of type kSbEventTypeLowMemory to the application.
  void InjectLowMemoryEvent();

#if SB_API_VERSION >= SB_WINDOW_SIZE_CHANGED_API_VERSION
  // Inject a window size change event.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void WindowSizeChanged(void* context, EventHandledCallback callback);
#endif  // SB_API_VERSION >= SB_WINDOW_SIZE_CHANGED_API_VERSION

  // Schedules an event into the event queue.  May be called from an external
  // thread.
  SbEventId Schedule(SbEventCallback callback,
                     void* context,
                     SbTimeMonotonic delay);

  // Cancels an event that was previously scheduled.  May be called from an
  // external thread.
  void Cancel(SbEventId id);

  // Handles receiving a new video frame of |player| from the media system. Only
  // used when the application needs to composite video frames with punch-out
  // video manually (should be rare). Will be called from an external thread.
  void HandleFrame(SbPlayer player,
                   const scoped_refptr<VideoFrame>& frame,
                   int z_index,
                   int x,
                   int y,
                   int width,
                   int height);

  // Registers a |callback| function that will be called when |Teardown| is
  // called.
  void RegisterTeardownCallback(TeardownCallback callback) {
    ScopedLock lock(callbacks_lock_);
    teardown_callbacks_.push_back(callback);
  }

 protected:
  // Initializes any systems that need initialization before application
  // start. Subclasses may override this method to run initialization code that
  // must be run before application start event is handled.
  virtual void Initialize() {}

  // Tears down any systems that need tearing down before application
  // termination. Subclasses may override this method to run teardown code that
  // must be run after the application stop event is handled.
  virtual void Teardown() {}

  // Does any platform-specific tearing-down AFTER the application has
  // processed the Suspend event, but before actual suspension.
  virtual void OnSuspend() {}

  // Does any platform-specific initialization BEFORE the application has
  // processed the Resume event.
  virtual void OnResume() {}

  // Subclasses may override this method to accept video frames from the media
  // system. Will be called from an external thread.
  virtual void AcceptFrame(SbPlayer /* player */,
                           const scoped_refptr<VideoFrame>& /* frame */,
                           int /* z_index */,
                           int /* x */,
                           int /* y */,
                           int /* width */,
                           int /* height */) {}

  // Blocks until the next event is available. Subclasses must implement this
  // method to provide events for the platform. Gives ownership to the caller.
  virtual Event* GetNextEvent() = 0;

  // Blocks until the next event is available, then dispatches the event to the
  // system event handler. Derived classes that override this should still use
  // |DispatchAndDelete| to maintain consistency of the application state.
  // Returns whether to keep servicing the event queue, i.e. false means to
  // abort the event queue.
  virtual bool DispatchNextEvent() { return DispatchAndDelete(GetNextEvent()); }

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

  // Sets the command-line parameters for the application. Used to support
  // system message pump-based implementations, which don't call |Run()|.
  void SetCommandLine(int argc, const char** argv) {
    command_line_.reset(new CommandLine(argc, argv));
  }

  // Sets the launch deep link string, if any, which is passed in the start
  // event that initializes and starts Cobalt.
  void SetStartLink(const char* start_link);

  // Returns whether the current thread is the Application thread.
  bool IsCurrentThread() const {
    return SbThreadIsEqual(thread_, SbThreadGetCurrent());
  }

  // Returns the current application state.
  State state() const { return state_; }

  // Returns the error level that the application has been stopped with. |0|
  // means "success" or at least "no error."
  int error_level() const { return error_level_; }

  // Returns whether the Start event should be sent in |Run| before entering the
  // event loop. Derived classes that return false must call |DispatchStart| at
  // some point.
  virtual bool IsStartImmediate() { return true; }

  // Synchronously dispatches a Start event to the system event handler. Must be
  // called on the main dispatch thread.
  void DispatchStart();

  // Returns whether the Preload event should be sent in |Run| before entering
  // the event loop. Derived classes that return true must call |Unpause| or
  // |DispatchStart| at some point.
  //
  // |IsPreloadImmediate|, if true, takes precedence over |IsStartImmediate|.
  virtual bool IsPreloadImmediate() { return false; }

  // Synchronously dispatches a Preload event to the system event handler. Must
  // be called on the main dispatch thread.
  void DispatchPreload();

  // Returns whether the '--preload' command-line argument is specified.
  bool HasPreloadSwitch();

  // Dispatches |event| to the system event handler, taking ownership of the
  // event. Checks for consistency with the current application state when state
  // events are dispatched. Returns whether to keep servicing the event queue,
  // i.e. false means to abort the event queue.
  bool DispatchAndDelete(Application::Event* event);

  // Calls registered Starboard teardown callbacks. This is called only by
  // |Run()| or directly by system message pump implementations at teardown.
  void CallTeardownCallbacks();

 private:
  // Creates an initial event type of either Start or Preload with the original
  // command line and deep link.
  Event* CreateInitialEvent(SbEventType type);

  // Internal workhorse of the main run loop.
  int RunLoop();

  // The single application instance.
  static Application* g_instance;

  // The error_level set by the last call to Stop().
  int error_level_;

  // The thread that this application was created on, which is assumed to be the
  // main thread.
  SbThread thread_;

  // CommandLine instance initialized in |Run|.
  scoped_ptr<CommandLine> command_line_;

  // The deep link included in the Start event sent to Cobalt. Initially NULL,
  // derived classes may set it during initialization using |SetStartLink|.
  char* start_link_;

  // The current state that the application is in based on what events it has
  // actually processed. Should only be accessed on the main thread.
  State state_;

  // Protect the teardown_callbacks_ vector.
  Mutex callbacks_lock_;

  // Callbacks that must be called when Teardown is called.
  std::vector<TeardownCallback> teardown_callbacks_;
};

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_APPLICATION_H_
