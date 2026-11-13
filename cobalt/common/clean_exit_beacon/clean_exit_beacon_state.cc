// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/common/clean_exit_beacon/clean_exit_beacon_state.h"

#include <atomic>

#include "base/command_line.h"

namespace cobalt {

namespace {

enum class State : int {
  kUnset = -1,
  kNotFlagged = 0,
  kFlagged = 1,
};

std::atomic<State> g_clean_exit_beacon_state{State::kUnset};

}  // namespace

void SetCleanExitBeaconFlagged(bool flagged) {
  g_clean_exit_beacon_state.store(
      flagged ? State::kFlagged : State::kNotFlagged,
      std::memory_order_relaxed);
}

bool IsCleanExitBeaconFlagged() {
  const State state = g_clean_exit_beacon_state.load(std::memory_order_relaxed);
  if (state != State::kUnset) {
    return state == State::kFlagged;
  }

  const bool flagged = base::CommandLine::InitializedForCurrentProcess() &&
                       base::CommandLine::ForCurrentProcess()->HasSwitch(
                           kCleanExitBeaconFlaggedSwitch);
  State expected = State::kUnset;
  if (g_clean_exit_beacon_state.compare_exchange_strong(
          expected, flagged ? State::kFlagged : State::kNotFlagged,
          std::memory_order_relaxed)) {
    return flagged;
  }
  return expected == State::kFlagged;
}

void ResetCleanExitBeaconFlaggedForTesting() {
  g_clean_exit_beacon_state.store(State::kUnset, std::memory_order_relaxed);
}

}  // namespace cobalt
