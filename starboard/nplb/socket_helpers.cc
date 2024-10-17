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

#include <ifaddrs.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "starboard/nplb/socket_helpers.h"

#include "starboard/common/log.h"
#include "starboard/common/socket.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/socket_waiter.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

int64_t TimedWait(SbSocketWaiter waiter) {
  int64_t start = CurrentMonotonicTime();
  SbSocketWaiterWait(waiter);
  return CurrentMonotonicTime() - start;
}

// Waits on the given waiter, and returns the elapsed time.
int64_t TimedWaitTimed(SbSocketWaiter waiter, int64_t timeout) {
  int64_t start = CurrentMonotonicTime();
  SbSocketWaiterWaitTimed(waiter, timeout);
  return CurrentMonotonicTime() - start;
}

}  // namespace nplb
}  // namespace starboard
