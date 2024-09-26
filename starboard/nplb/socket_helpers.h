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

// Returns a valid port number that can be bound to for use in nplb tests.
// This will always return the same port number.
int GetPortNumberForTests();

// Creates a TCP/IP server socket (sets Reuse Address option).
SbSocket CreateServerTcpSocket(SbSocketAddressType address_type);

// Creates a TCP/IP socket bound to all interfaces on the given port.
SbSocket CreateBoundTcpSocket(SbSocketAddressType address_type, int port);

// Creates a TCP/IP socket listening on all interfaces on the given port.
SbSocket CreateListeningTcpSocket(SbSocketAddressType address_type, int port);

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

// Socket operations may return specific (e.g. kSbSocketErrorConnectionReset) or
// general (e.g. kSbSocketErrorFailed) error codes, and while in some cases
// it may be important that we obtain a specific error message, in other cases
// it will just be used as a hint and so these methods are provided to make
// it easy to test against specific or general errors.
static inline bool SocketErrorIn(
    SbSocketError error,
    const std::vector<SbSocketError>& expected_set) {
  for (size_t i = 0; i < expected_set.size(); ++i) {
    if (expected_set[i] == error) {
      return true;
    }
  }
  return false;
}

#define EXPECT_SB_SOCKET_ERROR_IN(error, ...)        \
  do {                                               \
    EXPECT_TRUE(SocketErrorIn(error, {__VA_ARGS__})) \
        << "With " #error " = " << error;            \
  } while (false)

#define EXPECT_SB_SOCKET_ERROR_IS_ERROR(error)                          \
  do {                                                                  \
    EXPECT_FALSE(SocketErrorIn(error, {kSbSocketOk, kSbSocketPending})) \
        << "With " #error " = " << error;                               \
  } while (false)

#if !defined(COBALT_BUILD_TYPE_GOLD)
std::string GetSbSocketAddressTypeName(
    ::testing::TestParamInfo<SbSocketAddressType> info);
std::string GetSbSocketAddressTypePairName(
    ::testing::TestParamInfo<
        std::pair<SbSocketAddressType, SbSocketAddressType>> info);
std::string GetSbSocketAddressTypeFilterPairName(
    ::testing::TestParamInfo<
        std::pair<SbSocketAddressType, SbSocketResolveFilter>> info);
std::string GetSbSocketFilterAddressTypePairName(
    ::testing::TestParamInfo<
        std::pair<SbSocketResolveFilter, SbSocketAddressType>> info);
std::string GetSbSocketAddressTypeProtocolPairName(
    ::testing::TestParamInfo<std::pair<SbSocketAddressType, SbSocketProtocol>>
        info);
#endif  // #if !defined(COBALT_BUILD_TYPE_GOLD)

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_SOCKET_HELPERS_H_
