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

// A cross-platform base application engine that is used to manage the main
// event loop

#ifndef STARBOARD_SHARED_STARBOARD_APPLICATION_H_
#define STARBOARD_SHARED_STARBOARD_APPLICATION_H_

#include <pthread.h>

#include <memory>
#include <mutex>
#include <vector>

#include "starboard/common/condition_variable.h"
#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/time.h"
#include "starboard/event.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/types.h"
#include "starboard/window.h"

namespace starboard::shared::starboard {

// A small application framework for managing the application life-cycle, and
// dispatching events to the Starboard event handler, SbEventHandle.
class SB_EXPORT_ANDROID Application {
 public:
  typedef player::filter::VideoFrame VideoFrame;

  // Executes a SbEventHandle method callback.
  SbEventHandleCallback sb_event_handle_callback_ = NULL;

  // You can use a void(void *) function to signal that a state-transition event
  // has completed.
  typedef SbEventDataDestructor EventHandledCallback;

  // Signature for a function that will be called at the beginning of Teardown.
  typedef void (*TeardownCallback)(void);

  // Enumeration of states that the application can be in.
  enum State {
    // The initial Unstarted state.
    kStateUnstarted,

    // The normal foreground, fully-visible state after receiving the initial
    // START event or after FOCUS from Blurred.
    kStateStarted,

    // The background-but-visible or partially-obscured state after receiving a
    // BLUR event from Started or REVEAL event from Concealed.
    kStateBlurred,

    // The background-invisible state after receiving a CONCEAL event from
    // Blurred or UNFREEZE event from Frozen.
    kStateConcealed,

    // The fully-obscured or about-to-be-terminated state after receiving a
    // FREEZE event in Concealed.
    kStateFrozen,

    // The completely terminated state after receiving the STOP event in the
    // Frozen.
    kStateStopped,
  };

  // Structure to keep track of scheduled events, also used as the data argument
  // for kSbEventTypeScheduled Events.
  struct TimedEvent {
    TimedEvent(SbEventId eid, SbEventCallback func, void* data, int64_t delay)
        : id(eid),
          callback(func),
          context(data),
          target_time(delay + CurrentMonotonicTime()),
          canceled(false) {}

    SbEventId id;
    SbEventCallback callback;
    void* context;
    int64_t target_time;
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
    Event(SbEventType type,
          int64_t timestamp,
          void* data,
          SbEventDataDestructor destructor)
        : event(new SbEvent()), destructor(destructor), error_level(0) {
      event->type = type;
      event->timestamp = timestamp;
      event->data = data;
    }

    Event(SbEventType type, void* data, SbEventDataDestructor destructor)
        : event(new SbEvent()), destructor(destructor), error_level(0) {
      event->type = type;
      event->timestamp = CurrentMonotonicTime();
      event->data = data;
    }

    explicit Event(TimedEvent* data)
        : event(new SbEvent()),
          destructor(&DeleteDestructor<TimedEvent>),
          error_level(0) {
      event->type = kSbEventTypeScheduled;
      event->timestamp = CurrentMonotonicTime();
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

  explicit Application(SbEventHandleCallback sb_event_handle_callback);
  virtual ~Application();

  // Gets the current instance of the Application. This method CHECK application
  // instance is constructed.
  static Application* Get();

  // Runs the application with the current thread as the Main Starboard Thread,
  // blocking until application exit. This method will dispatch all appropriate
  // initialization and teardown events. Returns the resulting error level.
  int Run(CommandLine command_line, const char* link_data);
  int Run(CommandLine command_line);
  int Run(int argc, char** argv, const char* link_data) {
    return Run(CommandLine(argc, argv), link_data);
  }
  int Run(int argc, char** argv) { return Run(CommandLine(argc, argv)); }

// Prevents GetCommandLine from being redefined.  For example, Windows
// defines it to GetCommandLineW, which causes link errors.
#if defined(GetCommandLine)
#undef GetCommandLine
#endif  // defined(GetCommandLine)

  // Retrieves the CommandLine for the application.
  // NULL until Run() is called.
  const CommandLine* GetCommandLine();

  // Signals that the application should transition from STARTED to BLURRED as
  // soon as possible. Does nothing if already BLURRED or CONCEALED. May be
  // called from an external thread.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void Blur(void* context, EventHandledCallback callback);

  // Signals that the application should transition to STARTED as soon as
  // possible, moving through all required state transitions to get there. Does
  // nothing if already STARTED. May be called from an external thread.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void Focus(void* context, EventHandledCallback callback);

  // Signals that the application should transition to CONCEALED from BLURRED
  // as soon as possible, moving through all required state transitions to get
  // there. Does nothing if already CONCEALED, FROZEN, or STOPPED. May be called
  // from an external thread.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void Conceal(void* context, EventHandledCallback callback);

  // Signals that the application should transition to BLURRED from CONCEALED as
  // soon as possible, moving through all required state transitions to get
  // there. Does nothing if already STARTED or BLURRED. May be called from
  // an external thread.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void Reveal(void* context, EventHandledCallback callback);

  // Signals that the application should transition to FROZEN as soon as
  // possible, moving through all required state transitions to get there. Does
  // nothing if already FROZEN or STOPPED. May be called from an external
  // thread.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void Freeze(void* context, EventHandledCallback callback);

  // Signals that the application should transition to CONCEALED from FROZEN as
  // soon as possible. Does nothing if already CONCEALED, BLURRED, or STARTED.
  // May be called from an external thread.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void Unfreeze(void* context, EventHandledCallback callback);

  // Signals that the application should gracefully terminate as soon as
  // possible. Will transition through BLURRED, CONCEALED and FROZEN to STOPPED
  // as appropriate for the current state. May be called from an external
  // thread.
  void Stop(int error_level);

  // Injects a link event to the application with the given |link_data|, which
  // must be a null-terminated string. Makes a copy of |link_data|, so it only
  // has to live over the lifetime of the call to Link. May be called from an
  // external thread.
  void Link(const char* link_data);

  // Injects an event of type kSbEventTypeLowMemory to the application.
  void InjectLowMemoryEvent();

  void InjectOsNetworkDisconnectedEvent();
  void InjectOsNetworkConnectedEvent();
  void InjectDateTimeConfigurationChangedEvent();

  // Inject a window size change event.
  //
  // |context|: A context value to pass to |callback| on event completion. Must
  // not be NULL if callback is not NULL.
  // |callback|: A function to call on event completion, from the main thread.
  void WindowSizeChanged(void* context, EventHandledCallback callback);

  // Schedules an event into the event queue.  May be called from an external
  // thread.
  SbEventId Schedule(SbEventCallback callback, void* context, int64_t delay);

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
    std::lock_guard lock(callbacks_lock_);
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
  virtual void AcceptFrame(SbPlayer player,
                           const scoped_refptr<VideoFrame>& frame,
                           int z_index,
                           int x,
                           int y,
                           int width,
                           int height) {}

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

  // Gets the next time in microseconds that a TimedEvent is due. Returns
  // CurrentMonotonicTime() if the next TimedEvent is past due. Returns
  // std::numeric_limits<int64_t>::max() if there are no queued TimedEvents.
  virtual int64_t GetNextTimedEventTargetTime() = 0;

  // Sets the command-line parameters for the application. Used to support
  // system message pump-based implementations, which don't call |Run()|.
  void SetCommandLine(int argc, const char** argv) {
    command_line_.reset(new CommandLine(argc, argv));
  }

  void SetCommandLine(std::unique_ptr<CommandLine> command_line) {
    command_line_ = std::move(command_line);
  }

  // Sets the launch deep link string, if any, which is passed in the start
  // event that initializes and starts Cobalt.
  void SetStartLink(const char* start_link);

  // Returns whether the current thread is the Application thread.
  bool IsCurrentThread() const {
    return pthread_equal(thread_, pthread_self());
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
  void DispatchStart(int64_t timestamp);

  // Returns whether the Preload event should be sent in |Run| before entering
  // the event loop. Derived classes that return true must call |Unpause| or
  // |DispatchStart| at some point.
  //
  // |IsPreloadImmediate|, if true, takes precedence over |IsStartImmediate|.
  virtual bool IsPreloadImmediate() { return false; }

  // Synchronously dispatches a Preload event to the system event handler. Must
  // be called on the main dispatch thread.
  void DispatchPreload(int64_t timestamp);

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
  Event* CreateInitialEvent(SbEventType type, int64_t timestamp);

  // Internal workhorse of the main run loop.
  int RunLoop();

  // Do the actual processing of an event. This should only be called by
  // DispatchAndDelete().
  bool HandleEventAndUpdateState(Application::Event* event);

  // The error_level set by the last call to Stop().
  int error_level_;

  // The thread that this application was created on, which is assumed to be the
  // main thread.
  pthread_t thread_;

  // CommandLine instance initialized in |Run|.
  std::unique_ptr<CommandLine> command_line_;

  // The deep link included in the Start event sent to Cobalt. Initially NULL,
  // derived classes may set it during initialization using |SetStartLink|.
  char* start_link_;

  // The current state that the application is in based on what events it has
  // actually processed. Should only be accessed on the main thread.
  State state_;

  // Protect the teardown_callbacks_ vector.
  std::mutex callbacks_lock_;

  // Callbacks that must be called when Teardown is called.
  std::vector<TeardownCallback> teardown_callbacks_;
};

}  // namespace starboard::shared::starboard

#endif  // STARBOARD_SHARED_STARBOARD_APPLICATION_H_
