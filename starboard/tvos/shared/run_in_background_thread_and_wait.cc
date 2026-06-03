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

#include "starboard/tvos/shared/run_in_background_thread_and_wait.h"

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>

#include "starboard/common/check_op.h"

void RunInBackgroundThreadAndWait(std::function<void()>&& function) {
  // This function is supposed to be used in very specific circumstances: test
  // code that spawns one or more threads that need to make UI function calls
  // on the main thread via dispatch_sync() or dispatch_async().
  //
  // In this context, it is not possible _not_ to have a run loop here: calling
  // dispatch_sync() or using semaphores with GCD both have the same problem of
  // inevitably blocking the main thread without running the main event loop,
  // which leads to a deadlock.
  SB_CHECK_EQ(CFRunLoopGetMain(), CFRunLoopGetCurrent());
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
    function();
    CFRunLoopStop(CFRunLoopGetMain());
  });
  CFRunLoopRun();
}
