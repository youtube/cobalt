// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STATE_MACHINE_SHELL_H_
#define BASE_STATE_MACHINE_SHELL_H_

#include <stdint.h>

#include <algorithm>
#include <climits>
#include <queue>
#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"

namespace base {

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
//   * Subclass to define your state machine and event handlers.
//   * Include in another class as a private by-value member.
//   * Make sure your Event and State constants don't collide with the ones
//     predefined by this class.
//   * Synchronize access to the state machine.
//   * Avoid directly exposing or passing around state machines (wrap instead).
//     Handle() is not a great public interface.
//   * Prefer writing state machine event handlers over checking if the machine
//     IsIn() a particular state.
//   * Convert public methods into events, get into the state machine as quickly
//     as possible.
//   * You only need to define a state when you are going to leave the state
//     machine in a particular condition where events can occur.
//   * Create a superstate when you have an event you want to handle the same
//     way for a collection of states.
//   * When managing resources, create a state or superstate that represents the
//     acquisition of that resource, and release the resource in the kEventExit
//     handler.
//
// See the unittests for this class for a contrived example state machine
// implementation.
class BASE_EXPORT StateMachineShell {
 public:
  typedef uint32_t State;
  typedef uint32_t Event;

  // Representation of no specified state.
  static const State kStateNone = UINT_MAX;

  // Not a real state, but a value that can be returned from an event handler to
  // say that the event did not consume the event, and it should bubble up to
  // the parent state, if any.
  static const State kNotHandled = UINT_MAX - 1;

  // Pre-defined TOP state which contains all the other states in the machine.
  static const State kStateTop = UINT_MAX - 2;

  // Representation of no specified event.
  static const Event kEventNone = UINT_MAX;

  // Pre-defined event for entering a state.
  static const Event kEventEnter = UINT_MAX - 1;

  // Pre-defined event for exiting a state.
  static const Event kEventExit = UINT_MAX - 2;

  // Constructor with name. The name is helpful for debugging.
  explicit StateMachineShell(const std::string &name);
  virtual ~StateMachineShell() { }

  // Enters |kStateTop| and follows the initial substates down to the first
  // simple (childless) state found. Idempotent. Will happen automatically on
  // the first |Handle()| call, if not done explicitly.
  void Initialize();

  // Gets the name of this state machine, for logging purposes.
  const char *name() const {
    return name_.c_str();
  }

  // Gets a version number that increases monotonically with each state
  // transition (unless it overflows |uint64_t|). This can be useful for timers
  // and asynchronous events that only want to do their action if the state has
  // not changed since they were queued.
  //
  // The state version changes exactly once per transition. In other words, it
  // doesn't change for every Enter and Exit for a transition.
  uint64_t version() const {
    return version_;
  }

  // Gets the simplest current state that this state machine is in. To check if
  // the state machine is in a particular composite state, use |IsIn()|.
  State state() const {
    return state_;
  }

  // Reports whether the state machine is currently in the given state. A state
  // machine is considered to be "in" the current simple state, but also "in"
  // all superstates of the current simple state.
  bool IsIn(State state) const;

  // Gets a printable string for the given state.
  const char *GetStateString(State state) const;

  // Gets a printable string for the given event.
  const char *GetEventString(Event event) const;

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
  void Handle(Event event, void *data = NULL);

 protected:
  // Abstract method for subclasses to define the state hierarchy. If a state
  // has no other parent state, return |kStateTop|. Implementations do NOT need
  // to handle |kStateTop| or |kStateNone|. All user-defined states must be
  // descendents of |kStateTop|, or the behavior is undefined.
  virtual State GetUserParentState(State state) const = 0;

  // Abstract method for subclasses to define the initial substate of their
  // states, which is required for all complex states. Subclasses must include a
  // response for |kStateTop|, which will be followed upon initialization of the
  // machine. If a state is a simple state (no substates), return |kStateNone|.
  virtual State GetUserInitialSubstate(State state) const = 0;

  // Optional method for subclasses to define strings for their states, solely
  // used for debugging purposes. Implementations do NOT need to handle
  // |kStateTop| or |kStateNone| (or |kNotHandled| for that matter).
  virtual const char *GetUserStateString(State state) const { return NULL; }

  // Optional method for subclasses to define strings for their events, solely
  // used for debugging purposes. Implementations do NOT need to handle
  // |kEventEnter| or |kEventExit|.
  virtual const char *GetUserEventString(Event event) const { return NULL; }

  // Abstract method for subclasses to define handlers for events in given
  // states (including |kStateTop|). This method must return either:
  //   a) a state to transition to, meaning the event was handled and caused
  //      a transition
  //   b) |kStateNone| meaning the event was handled but no transition occurred
  //   c) |kNotHandled|, meaning the event should bubble up to the parent state
  //
  // When handling |kEventEnter| or |kEventExit|, the result must always be
  // |kStateNone| or |kNotHandled|. Enter and Exit events cannot cause
  // transitions.
  //
  // Implementations wishing to catch all unhandled events may do so in the
  // |kStateTop| state.
  //
  // This method is generally implemented as a nested switch statement, with the
  // outer switch on |state| and the inner switch on |event|. You may want to
  // break this apart into per-state or per-state-event functions for
  // readability and maintainability.
  virtual State HandleUserStateEvent(State state, Event event, void *data) = 0;

  // Subclasses can call this method to turn on logging. Logging is opt-in,
  // because it can be very verbose, and is likely only useful during
  // development of a particular state machine.
  void EnableLogging() {
    should_log_ = true;
  }

  // Method that subclasses can call inside HandleStateEvent to make the next
  // transition an external transition. If the current event handler does not
  // indicate a state transition, this method has no effect.
  //
  // For more on state transitions, see:
  // http://en.wikipedia.org/wiki/UML_state_machine#Transition_execution_sequence
  void MakeExternalTransition();

 private:
  // Gets the parent state of the given state.
  State GetParentState(State state) const;

  // Gets the initial substate of given state.
  State GetInitialSubstate(State state) const;

  // Handles all queued events until there are no more to run. Event handlers
  // may queue more events by calling Handle(), and this method will also
  // process all precipitate events.
  void HandleQueuedEvents();

  // Workhorse method for handling a single event. Event handling is queued by
  // binding the parameters to this method and queueing the resulting Closure.
  void HandleOneEvent(Event event, void *data);

  // Gets the path from |kStateTop| to the given state, storing it in the given
  // |out_path| array, up to |capacity| - 1 entries, with |out_depth| being set
  // to the number of valid states that would fit in the given array, if
  // specified. The entry at |out_depth| will always be set to |kStateNone|,
  // much like a null-terminated string.
  void GetPath(State state, size_t capacity, State *out_path,
               size_t *out_depth) const;

  // Transitions between the given source and target states, assuming that the
  // source state is in the current state path to TOP.
  //
  // See: http://en.wikipedia.org/wiki/UML_state_machine#Transition_execution_sequence
  void Transition(Event event, State source, State target, bool is_external);

  // Follows the initial substates from the current state until it reaches a
  // simple state.
  void FollowInitialSubstates();

  // Invokes the enter or exit handler on the current state. In debug, also
  // asserts that the return value matches the contract.
  void TransitionState(Event event);

  // --- Member Fields ---

  // The name of this state machine, for debugging purposes.
  const std::string name_;

  // The current state of this state machine.
  State state_;

  // The unique version of this state machine's state, updated on every
  // transition.
  uint64_t version_;

  // Queue of events to handle once the current event is done being
  // handled. Should always be empty unless |is_handling_| is true.
  std::queue<Closure> event_queue_;

  // Whether this state machine is actively handling an event. Used to detect
  // reentrant calls to |Handle()|.
  bool is_handling_;

  // Whether the next transition should be external. Can be set by handlers to
  // cause their precipitate transition to be an external transition. Only
  // meaningful while |is_handling_| is true.
  bool is_external_;

  // Whether the state machine has been initialized into its initial state
  // yet. Used to make |Initialize()| idempotent.
  bool is_initialized_;

  // Whether the state machine should DLOG information about state transitions.
  bool should_log_;
};

}  // namespace base

#endif  // BASE_CIRCULAR_BUFFER_SHELL_H_
