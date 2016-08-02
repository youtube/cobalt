// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "starboard/log.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

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

TEST(SbSocketWaiterWaitTimedTest, SunnyDay) {
  const int kBufSize = 1024;

  SbSocketWaiter waiter = SbSocketWaiterCreate();
  EXPECT_TRUE(SbSocketWaiterIsValid(waiter));

  ConnectedTrio trio =
      CreateAndConnect(GetPortNumberForTests(), kSocketTimeout);
  if (!SbSocketIsValid(trio.server_socket)) {
    ADD_FAILURE();
    return;
  }

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

TEST(SbSocketWaiterWaitTimedTest, RainyDayInvalidWaiter) {
  TimedWaitShouldNotBlock(kSbSocketWaiterInvalid, kSocketTimeout);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
