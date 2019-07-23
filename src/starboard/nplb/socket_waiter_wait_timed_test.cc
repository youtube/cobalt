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

#include <utility>

#include "starboard/common/log.h"
#include "starboard/common/socket.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket_waiter.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketWaiterWaitTimedTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

class PairSbSocketWaiterWaitTimedTest
    : public ::testing::TestWithParam<
          std::pair<SbSocketAddressType, SbSocketAddressType> > {
 public:
  SbSocketAddressType GetServerAddressType() { return GetParam().first; }
  SbSocketAddressType GetClientAddressType() { return GetParam().second; }
};

struct CallbackValues {
  int count;
  SbSocketWaiter waiter;
  SbSocket socket;
  void* context;
  int ready_interests;
};

void TestSocketWaiterCallback(SbSocketWaiter waiter,
                              SbSocket socket,
                              void* context,
                              int ready_interests) {
  CallbackValues* values = reinterpret_cast<CallbackValues*>(context);
  if (values) {
    ++values->count;
    values->waiter = waiter;
    values->socket = socket;
    values->context = context;
    values->ready_interests = ready_interests;
  }
  SbSocketWaiterWakeUp(waiter);
}

TEST_P(PairSbSocketWaiterWaitTimedTest, SunnyDay) {
  const int kBufSize = 1024;

  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  ConnectedTrio trio =
      CreateAndConnect(GetServerAddressType(), GetClientAddressType(),
                       GetPortNumberForTests(), kSocketTimeout);
  ASSERT_TRUE(SbSocketIsValid(trio.server_socket));

  // The client socket should be ready to write right away, but not read until
  // it gets some data.
  CallbackValues values = {0};
  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter, trio.client_socket, &values, &TestSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  TimedWaitShouldNotBlock(waiter, kSocketTimeout);

  // Even though we waited for no time, we should have gotten this callback.
  EXPECT_EQ(1, values.count);  // Check that the callback was called once.
  EXPECT_EQ(waiter, values.waiter);
  EXPECT_EQ(trio.client_socket, values.socket);
  EXPECT_EQ(&values, values.context);
  EXPECT_EQ(kSbSocketWaiterInterestWrite, values.ready_interests);

  // While we haven't written anything, we should block indefinitely, and
  // receive no read callbacks.
  values.count = 0;
  EXPECT_TRUE(SbSocketWaiterAdd(waiter, trio.client_socket, &values,
                                &TestSocketWaiterCallback,
                                kSbSocketWaiterInterestRead, false));

  TimedWaitShouldBlock(waiter, kSocketTimeout);

  EXPECT_EQ(0, values.count);

  // The client socket should become ready to read after we write some data to
  // it.
  char* send_buf = new char[kBufSize];
  int bytes_sent = SbSocketSendTo(trio.server_socket, send_buf, kBufSize, NULL);
  delete[] send_buf;
  EXPECT_LT(0, bytes_sent);

  TimedWaitShouldNotBlock(waiter, kSocketTimeout);

  EXPECT_EQ(1, values.count);
  EXPECT_EQ(waiter, values.waiter);
  EXPECT_EQ(trio.client_socket, values.socket);
  EXPECT_EQ(&values, values.context);
  EXPECT_EQ(kSbSocketWaiterInterestRead, values.ready_interests);

  EXPECT_TRUE(SbSocketDestroy(trio.server_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.client_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.listen_socket));
  EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
}

TEST_F(SbSocketWaiterWaitTimedTest, RainyDayInvalidWaiter) {
  TimedWaitShouldNotBlock(kSbSocketWaiterInvalid, kSocketTimeout);
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketWaiterWaitTimedTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketWaiterWaitTimedTest,
    ::testing::Values(
        std::make_pair(kSbSocketAddressTypeIpv4, kSbSocketAddressTypeIpv4),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv6),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv4)));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketWaiterWaitTimedTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketWaiterWaitTimedTest,
    ::testing::Values(std::make_pair(kSbSocketAddressTypeIpv4,
                                     kSbSocketAddressTypeIpv4)));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
