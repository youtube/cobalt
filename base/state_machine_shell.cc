// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/state_machine_shell.h"

#include "base/logging.h"

// --- Macros for common logging idioms ---

#define SM_STATE(s) GetStateString(s) << "(" << (s) << ")"
#define SM_EVENT(e) GetEventString(e) << "(" << (e) << ")"
#define SM_PREFIX \
    name_ << "-" << SM_STATE(state_) << "v" << version_ << ": "
#define SM_DLOG(level) DLOG(level) << SM_PREFIX

namespace base {

StateMachineShell::StateMachineShell(const std::string &name)
    : name_(name),
      state_(kStateNone),
      version_(0),
      is_handling_(false),
      is_external_(false),
      is_initialized_(false),
      should_log_(false) {
}

void StateMachineShell::Initialize() {
  if (is_initialized_) {
    return;
  }

  if (should_log_) {
    SM_DLOG(INFO) << "INITIALIZING";
  }
  DCHECK_EQ(state_, kStateNone);
  is_initialized_ = true;
  FollowInitialSubstates();
}

bool StateMachineShell::IsIn(State state) const {
  State currentState = state_;
  while (currentState != kStateNone) {
    if (state == currentState) {
      return true;
    }

    currentState = GetParentState(currentState);
  }

  return false;
}

const char *StateMachineShell::GetStateString(State state) const {
  switch (state) {
    case kStateTop:
      return "TOP";
    case kStateNone:
      return "<none>";
    case kNotHandled:
      return "<notHandled>";
    default: {
      const char *user = GetUserStateString(state);
      if (user) {
        return user;
      }
      return "UNKNOWN_STATE";
    }
  }
}

const char *StateMachineShell::GetEventString(Event event) const {
  switch (event) {
    case kEventEnter:
      return "ENTER";
    case kEventExit:
      return "EXIT";
    case kEventNone:
      return "<none>";
    default: {
      const char *user = GetUserEventString(event);
      if (user) {
        return user;
      }
      return "UNKNOWN_EVENT";
    }
  }
}

void StateMachineShell::Handle(Event event, void *data) {
  Initialize();

  if (is_handling_) {
    if (should_log_) {
      SM_DLOG(INFO) << "QUEUEING " << SM_EVENT(event);
    }

    event_queue_.push(Bind(&StateMachineShell::HandleOneEvent, Unretained(this),
                           event, Unretained(data)));
    return;
  }

  HandleOneEvent(event, data);
  HandleQueuedEvents();
}

void StateMachineShell::MakeExternalTransition() {
  if (!is_handling_) {
    SM_DLOG(FATAL) << "Can only make an external transition while handling an "
                   << "event.";
    return;
  }

  is_external_ = true;
}

StateMachineShell::State StateMachineShell::GetParentState(State state) const {
  switch (state) {
    case kStateTop:
    case kStateNone:
    case kNotHandled:
      return kStateNone;
    default:
      return GetUserParentState(state);
  }
}

StateMachineShell::State
StateMachineShell::GetInitialSubstate(State state) const {
  if (state == kStateNone) {
    return kStateTop;
  }

  return GetUserInitialSubstate(state);
}

void StateMachineShell::HandleQueuedEvents() {
  while (!event_queue_.empty()) {
    event_queue_.front().Run();
    event_queue_.pop();
  }
}

void StateMachineShell::HandleOneEvent(Event event, void *data) {
  if (should_log_) {
    SM_DLOG(INFO) << "HANDLING " << SM_EVENT(event);
  }

  is_handling_ = true;

  // Walk up the hierarchy from the simplest current state, looking for event
  // handlers, stopping at the first state that decides to handle the event.
  State source = state_;
  State target = kStateNone;
  bool handled = false;
  bool is_external = false;
  while (source != kStateNone) {
    is_external_ = false;
    State result = HandleUserStateEvent(source, event, data);
    if (result == kNotHandled) {
      // Result was not handled, so we continue up the state chain.
      source = GetParentState(source);
      continue;
    }

    is_external = is_external_;
    target = result;
    handled = true;
    break;
  }

  // Log that the event was completely unhandled, because that's kinda weird.
  if (!handled) {
    SM_DLOG(WARNING) << "Event " << SM_EVENT(event) << " was unhandled.";
  }

  // If a transition was triggered, execute it.
  if (target != kStateNone) {
    Transition(event, source, target, is_external);
  }

  is_handling_ = false;
}

// Returns the index of |state| in |kStateNone|-terminated |path|, or -1 if not
// found.
static int IndexOf(StateMachineShell::State state,
                   const StateMachineShell::State *path) {
  int i;
  for (i = 0; path[i] != state; ++i) {
    if (path[i] == StateMachineShell::kStateNone) {
      return -1;
    }
  }

  return i;
}

// Finds the least common ancestor between the two |kStateNone|-terminated state
// paths. Since all states are expected to be children of a single TOP state,
// these paths are required to each have at least the TOP state in them.
static StateMachineShell::State FindLeastCommonAncestor(
    const StateMachineShell::State *path_a,
    const StateMachineShell::State *path_b) {
  size_t i = 0;
  while (true) {
    if (path_a[i] != path_b[i] || path_a[i] == StateMachineShell::kStateNone ||
        path_b[i] == StateMachineShell::kStateNone) {
      DCHECK_GT(i, 0) << "All states must be children of kStateTop";
      return path_a[i - 1];
    }
    ++i;
  }

  NOTREACHED();
}

void StateMachineShell::GetPath(State state, size_t capacity, State *out_path,
                                size_t *out_depth) const {
  if (capacity == 0) {
    SM_DLOG(FATAL) << "capacity must be > 0";
    if (out_depth) {
      *out_depth = 0;
    }
    return;
  }

  size_t max_depth = capacity - 1;
  size_t depth = 0;
  while (state != kStateNone && depth < max_depth) {
    out_path[depth] = state;
    ++depth;
    state = GetParentState(state);
  }

  // Reverse so the path is in ascending depth order.
  std::reverse(out_path, out_path + depth);

  // Ensure proper termination
  out_path[depth] = kStateNone;
  if (out_depth) {
    *out_depth = depth;
  }
}

void StateMachineShell::Transition(Event event, State source, State target,
                                   bool is_external) {
  if (should_log_) {
    SM_DLOG(INFO) << SM_EVENT(event) << " caused " << SM_STATE(source) << " -> "
                  << SM_STATE(target) << (is_external ? " (external)" : "");
  }

  // Define a reasonable max depth so we don't have to heap allocate. I've never
  // seen an HSM as deep as whatever arbitrary value this is defined to be at
  // the moment.
  static const size_t kMaxDepth = 16;
  State source_path[kMaxDepth];
  size_t source_depth;
  GetPath(source, kMaxDepth, source_path, &source_depth);
  State target_path[kMaxDepth];
  size_t target_depth;
  GetPath(target, kMaxDepth, target_path, &target_depth);
  State least_common_ancestor =
      FindLeastCommonAncestor(source_path, target_path);

  // External transitions must exit the source state and enter the target
  // state, so if the LCA is one of the two, we need to go one level up.
  if (is_external && (least_common_ancestor == source ||
                      least_common_ancestor == target)) {
    least_common_ancestor = GetParentState(least_common_ancestor);
  }

  // Unwind (exit) states up to the closest common ancestor.
  while (state_ != kStateNone && state_ != least_common_ancestor) {
    TransitionState(kEventExit);
    state_ = GetParentState(state_);
  }

  // Update version before we wind down to the target.
  ++version_;

  // Wind (enter) states down to the target state.
  size_t next_depth = IndexOf(state_, target_path) + 1;
  DCHECK_LE(next_depth, target_depth);
  while (next_depth < target_depth) {
    state_ = target_path[next_depth];
    TransitionState(kEventEnter);
    ++next_depth;
  }

  FollowInitialSubstates();
}

void StateMachineShell::FollowInitialSubstates() {
  while (true) {
    State substate = GetInitialSubstate(state_);
    if (substate == kStateNone) {
      break;
    }
    state_ = substate;
    TransitionState(kEventEnter);
  }
}

void StateMachineShell::TransitionState(Event event) {
  if (should_log_) {
    SM_DLOG(INFO) << GetEventString(event);
  }

  State result = HandleUserStateEvent(state_, event, NULL);
  DCHECK(result == kStateNone || result == kNotHandled) << SM_PREFIX;
}

}  // namespace base
