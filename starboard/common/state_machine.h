// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STARBOARD_COMMON_STATE_MACHINE_H_
#define STARBOARD_COMMON_STATE_MACHINE_H_

#include <queue>
#include <string>

#include "starboard/common/optional.h"
#include "starboard/configuration.h"
#include "starboard/export.h"

#ifdef __cplusplus
namespace starboard {

// An approximate implementation of a run-to-completion (RTC) Hierarchical State
// Machine (HSM), supporting most UML Statechart semantics.
//
// Some good background information on UML Statecharts, mostly written by Miro
// Samek:
// http://en.wikipedia.org/wiki/UML_state_machine
//
// And Miro Samek's 3-part article on practical UML Statecharts:
// http://www.embedded.com/design/prototyping-and-development/4008247/A-crash-course-in-UML-state-machines-Part-1
//
// This class does not provide any intrinsic support for "orthogonal regions,"
// "extended state," or "deferred events." "Guard conditions" are easily
// implemented by the client directly in the event handler. It also minorly
// deviates from the UML Statechart specification by calling transition handlers
// before exiting the current states. This is because this implementation uses a
// simplification which combines the transition handlers and the
// state-event-transition map into a single function (HandleUserStateEvent).
//
// This class is not thread-safe. It is the user's responsibility to synchronize
// calls into the state machine, either with locks or by simply using from a
// single thread.
//
// Terse suggestions for using this class:
//   * Use the templated StateMachine wrapper class instead of this class
//     directly.
//   * Define two enums, one for your states, and one for your events.
//   * Subclass to define your state machine and event handlers.
//   * Avoid directly exposing or passing around state machines (wrap instead).
//     Handle() is not a great public interface.
//   * Include your state machine in another class as a private by-value member.
//   * Synchronize access to the state machine.
//   * Prefer writing state machine event handlers over checking if the machine
//     IsIn() a particular state.
//   * Convert public methods into events, get into the state machine as quickly
//     as possible.
//   * You only need to define a state when you are going to leave the state
//     machine in a particular condition where events can occur.
//   * Create a superstate when you have an event you want to handle the same
//     way for a collection of states.
//   * When managing resources, create a state or superstate that represents the
//     acquisition of that resource, and release the resource in the Exit
//     handler.
//
// Some Definitions:
//   Simple State      - A State with no substates. The state machine is always
//                       left in exactly one Simple State.
//   Composite State   - A State with at least one substate.
//   Guard Condition   - An external condition on which an event handler
//                       branches.
//   Run-To-Completion - A property specifying that the state machine handles
//                       one event at a time, and no events are handled until
//                       the previous event is done being handled.
//
// See the unittests for this class for a contrived example state machine
// implementation.
class StateMachineBase {
 public:
  // --- Nested Types and Constants ---

  typedef uint32_t State;
  typedef uint32_t Event;

  // --- Public Methods ---

  // Constructor with name. The name is helpful for debugging.
  explicit StateMachineBase(const std::string& name);
  virtual ~StateMachineBase() {}

  // Enters the initial state, as specified by |GetUserInitialState()| and
  // follows the initial substates down to the first simple (childless) state
  // found. Idempotent. Will happen automatically on the first |Handle()| call,
  // if not done explicitly.
  void Initialize();

  // Gets the name of this state machine, for logging purposes.
  const char* name() const { return name_.c_str(); }

  // Gets a version number that increases monotonically with each state
  // transition (unless it overflows |uint64_t|). This can be useful for timers
  // and asynchronous events that only want to do their action if the state has
  // not changed since they were queued.
  //
  // The state version changes exactly once per transition. In other words, it
  // doesn't change for every Enter and Exit for a transition.
  uint64_t version() const { return version_; }

  // Gets the simplest current state that this state machine is in. To check if
  // the state machine is in a particular composite state, use |IsIn()|. Returns
  // null if uninitialized.
  optional<State> state() const { return state_; }

  // Reports whether the state machine is currently in the given state. A state
  // machine is considered to be "in" the current simple state, but also "in"
  // all superstates of the current simple state.
  bool IsIn(State state) const;

  // Gets a printable string for the given state.
  const char* GetStateString(optional<State> state) const;

  // Gets a printable string for the given event.
  const char* GetEventString(optional<Event> event) const;

  // Handles the given event in the context of the state machine's current
  // state, executing the appropriate event handlers and performing any
  // precipitate state transitions. If called reentrantly, the event will be
  // queued until the current event has run to completion.
  //
  // |data| is a way to pass auxiliary data to the event handler. No retention
  // or ref-counting is done on that data, it is simply passed through to the
  // event handler. Since |Handle()| will queue the event if (and only if)
  // called from an event handler, it is up to the caller to ensure the lifetime
  // of whatever is passed in here.
  void Handle(Event event, void* data = NULL);

 protected:
  // --- Protected Nested Types ---

  // A type that can be returned from a state-event handler, which will be
  // coerced into the appropriate Result structure.
  enum HandledState {
    // The event handler returns this to say that the handler consume the event,
    // but caused no state transition.
    kHandled,

    // The event handler returns this to say that the handler did not consume
    // the event, and it should bubble up to the parent state, if any.
    kNotHandled,
  };

  // Structure that handlers return, allowing them to cause state transitions or
  // prevent the event from bubbling. State-event handlers may just return a
  // HandledState or a state to transition to, and it will be coerced into this
  // structure.
  struct Result {
    // Default constructor is unhandled.
    Result() : is_transition(false), is_external(false), is_handled(false) {}

    // The no-transition constructor. Non-explicit so that the implementor of
    // |HandleUserStateEvent()| just needs to return a |HandledState| and it
    // does the right thing.
    Result(HandledState handled)  // NOLINT(runtime/explicit)
        : is_transition(false),
          is_external(false),
          is_handled(handled == kHandled) {}

    // The state transition constructor. This implies that the event was
    // handled. Non-explicit so that the implementor of |HandleUserStateEvent()|
    // just needs to return a State and it does a non-external transition.
    Result(State transition_target,
           bool external = false)  // NOLINT(runtime/explicit)
        : target(transition_target),
          is_transition(true),
          is_external(external),
          is_handled(true) {}

    Result& operator=(HandledState rhs) {
      target = nullopt;
      is_transition = false;
      is_external = false;
      is_handled = (rhs == kHandled);
      return *this;
    }

    Result& operator=(State transition_target) {
      target = transition_target;
      is_transition = true;
      is_external = false;
      is_handled = true;
      return *this;
    }

    // State to transition to. Only valid if is_transition is true.
    optional<State> target;

    // Whether this result indicates a state transition.
    bool is_transition;

    // Whether the specified transition is external. Only meaningful if
    // is_transition is true.
    //
    // For more on state transitions, see:
    // http://en.wikipedia.org/wiki/UML_state_machine#Transition_execution_sequence
    bool is_external;

    // Whether the event was handled by the handler. If true, consumes the
    // event, and prevents bubbling up to superstates. False propagates the
    // event to superstates.
    bool is_handled;
  };

  struct EventWithData {
    EventWithData(Event event, void* data) : event(event), data(data) {}
    Event event;
    void* data;
  };

  // --- Implementation Interface for Subclasses ---

  // Abstract method for subclasses to define the state hierarchy. It is
  // recommended that all user-defined states be descendents of a single "top"
  // state.
  virtual optional<State> GetUserParentState(State state) const = 0;

  // Abstract method for subclasses to define the initial substate of their
  // states, which is required for all complex states. If a state is a simple
  // state (no substates), returns null.
  virtual optional<State> GetUserInitialSubstate(State state) const = 0;

  // Abstract method for subclasses to define the initial (top) state of their
  // state machine. This must be a state that has no parent. This state will be
  // entered upon calling |Initialize()|.
  virtual State GetUserInitialState() const = 0;

  // Optional method for subclasses to define strings for their states, solely
  // used for debugging purposes.
  virtual const char* GetUserStateString(State state) const { return NULL; }

  // Optional method for subclasses to define strings for their events, solely
  // used for debugging purposes.
  virtual const char* GetUserEventString(Event event) const { return NULL; }

  // Abstract method for subclasses to define handlers for events in given
  // states. This method must return either:
  //   a) a state to transition to, meaning the event was handled and caused
  //      a transition
  //   b) |kHandled| meaning the event was handled but no transition occurred
  //   c) |kNotHandled|, meaning the event should bubble up to the parent state
  //   d) an explicit Result structure, mainly for external transitions.
  //
  // Implementations wishing to catch all unhandled events may do so in their
  // top state.
  //
  // This method is generally implemented as a nested switch statement, with the
  // outer switch on |state| and the inner switch on |event|. You may want to
  // break this apart into per-state or per-state-event functions for
  // readability and maintainability.
  virtual Result HandleUserStateEvent(State state, Event event, void* data) = 0;

  // Abstract method for subclasses to define state entry behaviors.
  virtual void HandleUserStateEnter(State state) = 0;

  // Abstract method for subclasses to define state exit behaviors.
  virtual void HandleUserStateExit(State state) = 0;

  // --- Helper Methods for Subclasses ---

  // Subclasses can call this method to turn on logging. Logging is opt-in,
  // because it can be very verbose, and is likely only useful during
  // development of a particular state machine.
  void EnableLogging() { should_log_ = true; }

 private:
  // --- Private Helper Methods ---

  // Gets the parent state of the given state.
  optional<State> GetParentState(optional<State> state) const;

  // Gets the initial substate of given state.
  optional<State> GetInitialSubstate(optional<State> state) const;

  // Handles all queued events until there are no more to run. Event handlers
  // may queue more events by calling |Handle()|, and this method will also
  // process all precipitate events.
  void HandleQueuedEvents();

  // Workhorse method for handling a single event.
  void HandleOneEvent(Event event, void* data);

  // Gets the path from the Top state to the given |state|, storing it in the
  // given |out_path| array, up to |max_depth| entries. If specified,
  // |out_depth| will be set to the number of valid states that fit in the given
  // array.
  void GetPath(State state,
               size_t max_depth,
               State* out_path,
               size_t* out_depth) const;

  // Transitions between the given source and target states, assuming that the
  // source state is in the current state path to the Top state. The source
  // state is the state whose handler generated the transition.
  //
  // See:
  // http://en.wikipedia.org/wiki/UML_state_machine#Transition_execution_sequence
  void Transition(Event event, State source, State target, bool is_external);

  // Follows the initial substates from the current state until it reaches a
  // simple state.
  void FollowInitialSubstates();

  // Enters the given state.
  void EnterState(State state);

  // Exits the current state to its parent.
  void ExitCurrentState();

  // --- Members ---

  // The name of this state machine, for debugging purposes.
  const std::string name_;

  // The current state of this state machine. Null until initialized.
  optional<State> state_;

  // The unique version of this state machine's state, updated on every
  // transition.
  uint64_t version_;

  // Queue of events to handle once the current event is done being
  // handled. Should always be empty unless |is_handling_| is true.
  std::queue<EventWithData> event_queue_;

  // Whether this state machine is actively handling an event. Used to detect
  // reentrant calls to |Handle()|.
  bool is_handling_;

  // Whether the state machine has been initialized into its initial state
  // yet. Used to make |Initialize()| idempotent.
  bool is_initialized_;

  // Whether the state machine should log information about state transitions.
  bool should_log_;
};

// A convenience template wrapper for StateMachineBase. See the above class
// for complete documentation. Basically, you define your states and events as
// two enums, and then pass them as template args to this template class. Your
// state machine should then subclass this template class. It then does the work
// of casting and converting back and forth from your enums to
// StateMachineBase's numeric State and Event definitions.
//
// All the methods in this class, protected and public, match the description
// and behavioral contracts of the equivalently named method in
// StateMachineBase.
template <typename StateEnum, typename EventEnum>
class StateMachine {
 public:
  // --- Nested Types and Constants ---

  explicit StateMachine(const std::string& name) : machine_(this, name) {}
  virtual ~StateMachine() {}

  void Initialize() { machine_.Initialize(); }

  const char* name() const { return machine_.name(); }

  uint64_t version() const { return machine_.version(); }

  optional<StateEnum> state() const {
    optional<BaseState> wrappedState = machine_.state();
    return (wrappedState ? static_cast<StateEnum>(*wrappedState)
                         : optional<StateEnum>());
  }

  bool IsIn(StateEnum state) const {
    return machine_.IsIn(static_cast<BaseState>(state));
  }

  const char* GetStateString(optional<StateEnum> state) const {
    return machine_.GetStateString(state ? static_cast<BaseState>(*state)
                                         : optional<BaseState>());
  }

  const char* GetEventString(optional<EventEnum> event) const {
    return machine_.GetEventString(event ? static_cast<BaseEvent>(*event)
                                         : optional<BaseEvent>());
  }

  void Handle(EventEnum event, void* data = NULL) {
    machine_.Handle(static_cast<BaseEvent>(event), data);
  }

 protected:
  // See the other HandledState in StateMachineBase.
  enum HandledState {
    kHandled,
    kNotHandled,
  };

  // See the other Result in StateMachineBase.
  struct Result {
    Result(HandledState handled)  // NOLINT(runtime/explicit)
        : target(),
          is_transition(false),
          is_external(false),
          is_handled(handled == kHandled) {}

    Result(StateEnum transition_target,
           bool external = false)  // NOLINT(runtime/explicit)
        : target(transition_target),
          is_transition(true),
          is_external(external),
          is_handled(true) {}

    Result& operator=(HandledState rhs) {
      target = nullopt;
      is_transition = false;
      is_external = false;
      is_handled = (rhs == kHandled);
      return *this;
    }

    Result& operator=(StateEnum transition_target) {
      target = transition_target;
      is_transition = true;
      is_external = false;
      is_handled = true;
      return *this;
    }

    optional<StateEnum> target;
    bool is_transition;
    bool is_external;
    bool is_handled;
  };

  virtual optional<StateEnum> GetUserParentState(StateEnum state) const = 0;
  virtual optional<StateEnum> GetUserInitialSubstate(StateEnum state) const = 0;
  virtual StateEnum GetUserInitialState() const = 0;
  virtual const char* GetUserStateString(StateEnum state) const { return NULL; }
  virtual const char* GetUserEventString(EventEnum event) const { return NULL; }
  virtual Result HandleUserStateEvent(StateEnum state,
                                      EventEnum event,
                                      void* data) = 0;
  virtual void HandleUserStateEnter(StateEnum state) = 0;
  virtual void HandleUserStateExit(StateEnum state) = 0;

  void EnableLogging() { machine_.EnableLoggingPublic(); }

 private:
  typedef StateMachineBase::State BaseState;
  typedef StateMachineBase::Event BaseEvent;

  // Private contained subclass that forwards and adapts all virtual methods
  // into this class's equivalent virtual methods.
  class WrappedMachine : public StateMachineBase {
   public:
    WrappedMachine(StateMachine<StateEnum, EventEnum>* wrapper,
                   const std::string& name)
        : StateMachineBase(name), wrapper_(wrapper) {}

    optional<State> GetUserParentState(State state) const override {
      optional<StateEnum> result =
          wrapper_->GetUserParentState(static_cast<StateEnum>(state));
      return (result ? static_cast<State>(*result) : optional<State>());
    }

    optional<State> GetUserInitialSubstate(State state) const override {
      optional<StateEnum> result =
          wrapper_->GetUserInitialSubstate(static_cast<StateEnum>(state));
      return (result ? static_cast<State>(*result) : optional<State>());
    }

    State GetUserInitialState() const override {
      return static_cast<State>(wrapper_->GetUserInitialState());
    }

    const char* GetUserStateString(State state) const override {
      return wrapper_->GetUserStateString(static_cast<StateEnum>(state));
    }

    const char* GetUserEventString(Event event) const override {
      return wrapper_->GetUserEventString(static_cast<EventEnum>(event));
    }

    Result HandleUserStateEvent(State state,
                                Event event,
                                void* data) override {
      typename StateMachine<StateEnum, EventEnum>::Result result =
          wrapper_->HandleUserStateEvent(static_cast<StateEnum>(state),
                                         static_cast<EventEnum>(event), data);
      if (result.is_transition) {
        return Result(static_cast<State>(*result.target), result.is_external);
      }

      return result.is_handled ? kHandled : kNotHandled;
    }

    void HandleUserStateEnter(State state) override {
      wrapper_->HandleUserStateEnter(static_cast<StateEnum>(state));
    }

    void HandleUserStateExit(State state) override {
      wrapper_->HandleUserStateExit(static_cast<StateEnum>(state));
    }

    // A public exposure of EnableLogging so the wrapper can access it. Since
    // this class is private to the wrapper, it is the only one who can see it.
    void EnableLoggingPublic() { EnableLogging(); }

   private:
    StateMachine<StateEnum, EventEnum>* wrapper_;
  };

  WrappedMachine machine_;
};

}  // namespace starboard
#endif  // __cplusplus

#endif  // STARBOARD_COMMON_STATE_MACHINE_H_
