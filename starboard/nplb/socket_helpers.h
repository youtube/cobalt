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

#ifndef STARBOARD_NPLB_SOCKET_HELPERS_H_
#define STARBOARD_NPLB_SOCKET_HELPERS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starboard/common/socket.h"
#include "starboard/socket_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

const int64_t kSocketTimeout = 200'000;  // 200ms

// Waits on the given waiter, and returns the elapsed time in microseconds.
int64_t TimedWait(SbSocketWaiter waiter);

// Waits on the given waiter, and returns the elapsed time in microseconds.
int64_t TimedWaitTimed(SbSocketWaiter waiter, int64_t timeout);

// Waits on the given waiter, and checks that it blocked between
// [lower_usec, upper_usec).
static inline void WaitShouldBlockBetween(SbSocketWaiter waiter,
                                          int64_t lower_usec,
                                          int64_t upper_usec) {
  int64_t time = TimedWait(waiter);
  EXPECT_GT(upper_usec, time);
  EXPECT_LE(lower_usec, time);
}

// Waits on the given waiter, and checks that it did not block for very long.
static inline void WaitShouldNotBlock(SbSocketWaiter waiter) {
  WaitShouldBlockBetween(waiter, 0, kSocketTimeout);
}

// Waits on the given waiter, and checks that it did not block for the given
// timeout.
static inline void TimedWaitShouldNotBlock(SbSocketWaiter waiter,
                                           int64_t timeout) {
  EXPECT_GT(timeout, TimedWaitTimed(waiter, timeout));
}

// Waits on the given waiter, and checks that it did block for at least the
// given timeout.
static inline void TimedWaitShouldBlock(SbSocketWaiter waiter,
                                        int64_t timeout) {
  EXPECT_LE(timeout, TimedWaitTimed(waiter, timeout));
}

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_SOCKET_HELPERS_H_
