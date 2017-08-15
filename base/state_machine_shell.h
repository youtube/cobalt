// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STATE_MACHINE_SHELL_H_
#define BASE_STATE_MACHINE_SHELL_H_

#include "starboard/common/state_machine.h"

namespace base {
using ::starboard::StateMachineBase;
using ::starboard::StateMachine;

template <typename StateEnum, typename EventEnum>
using StateMachineShell = StateMachine<StateEnum, EventEnum>;

}  // namespace base

#endif  // BASE_CIRCULAR_BUFFER_SHELL_H_
