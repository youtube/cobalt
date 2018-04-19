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

#include <utility>

#include "starboard/log.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/nplb/thread_helpers.h"
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbSocketWaiterWaitTest
    : public ::testing::TestWithParam<SbSocketAddressType> {
 public:
  SbSocketAddressType GetAddressType() { return GetParam(); }
};

class PairSbSocketWaiterWaitTest
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

void FailSocketWaiterCallback(SbSocketWaiter waiter,
                              SbSocket socket,
                              void* context,
                              int ready_interests) {
  ADD_FAILURE() << __FUNCTION__ << " was called.";
}

TEST_P(PairSbSocketWaiterWaitTest, SunnyDay) {
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

  // This add should do nothing, since it is already registered. Testing here
  // because we can see if it fires a write to the proper callback, then it
  // properly ignored this second set of parameters.
  EXPECT_FALSE(SbSocketWaiterAdd(waiter, trio.client_socket, &values,
                                 &FailSocketWaiterCallback,
                                 kSbSocketWaiterInterestRead, false));

  WaitShouldNotBlock(waiter);

  EXPECT_EQ(1, values.count);  // Check that the callback was called once.
  EXPECT_EQ(waiter, values.waiter);
  EXPECT_EQ(trio.client_socket, values.socket);
  EXPECT_EQ(&values, values.context);
  EXPECT_EQ(kSbSocketWaiterInterestWrite, values.ready_interests);

  // Try again to make sure writable sockets are still writable
  values.count = 0;
  EXPECT_TRUE(SbSocketWaiterAdd(
      waiter, trio.client_socket, &values, &TestSocketWaiterCallback,
      kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite, false));

  WaitShouldNotBlock(waiter);

  EXPECT_EQ(1, values.count);  // Check that the callback was called once.
  EXPECT_EQ(waiter, values.waiter);
  EXPECT_EQ(trio.client_socket, values.socket);
  EXPECT_EQ(&values, values.context);
  EXPECT_EQ(kSbSocketWaiterInterestWrite, values.ready_interests);

  // The client socket should become ready to read after we write some data to
  // it.
  values.count = 0;
  EXPECT_TRUE(SbSocketWaiterAdd(waiter, trio.client_socket, &values,
                                &TestSocketWaiterCallback,
                                kSbSocketWaiterInterestRead, false));

  // Now we can send something to trigger readability.
  char* send_buf = new char[kBufSize];
  int bytes_sent = SbSocketSendTo(trio.server_socket, send_buf, kBufSize, NULL);
  delete[] send_buf;
  EXPECT_LT(0, bytes_sent);

  WaitShouldNotBlock(waiter);

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

struct AlreadyReadyContext {
  AlreadyReadyContext()
      : waiter(kSbSocketWaiterInvalid),
        a_write_result(false),
        a_read_result(false),
        b_result(false) {}
  ~AlreadyReadyContext() {
    if (SbSocketIsValid(trio.listen_socket)) {
      EXPECT_TRUE(SbSocketDestroy(trio.listen_socket));
    }
    if (SbSocketIsValid(trio.server_socket)) {
      EXPECT_TRUE(SbSocketDestroy(trio.server_socket));
    }
    if (SbSocketIsValid(trio.client_socket)) {
      EXPECT_TRUE(SbSocketDestroy(trio.client_socket));
    }
    EXPECT_TRUE(SbSocketWaiterDestroy(waiter));
  }

  SbSocketWaiter waiter;
  ConnectedTrio trio;
  Semaphore wrote_a_signal;
  bool a_write_result;
  bool a_read_result;
  Semaphore write_b_signal;
  bool b_result;
  Semaphore wrote_b_signal;
};

const char kAData[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
const char kBData[] = "bb";

void* AlreadyReadyEntryPoint(void* param) {
  AlreadyReadyContext* context = reinterpret_cast<AlreadyReadyContext*>(param);

  context->a_write_result =
      WriteBySpinning(context->trio.server_socket, kAData,
                      SB_ARRAY_SIZE_INT(kAData), kSocketTimeout);
  context->wrote_a_signal.Put();

  context->write_b_signal.Take();
  context->b_result =
      WriteBySpinning(context->trio.server_socket, kBData,
                      SB_ARRAY_SIZE_INT(kBData), kSocketTimeout);
  context->wrote_b_signal.Put();
  return NULL;
}

void WakeUpSocketWaiterCallback(SbSocketWaiter waiter,
                                SbSocket socket,
                                void* param,
                                int ready_interests) {
  SbSocketWaiterWakeUp(waiter);
}

void AlreadyReadySocketWaiterCallback(SbSocketWaiter waiter,
                                      SbSocket socket,
                                      void* param,
                                      int ready_interests) {
  AlreadyReadyContext* context = reinterpret_cast<AlreadyReadyContext*>(param);
  // Read in the A data.
  char buffer[SB_ARRAY_SIZE_INT(kAData)];
  context->a_read_result =
      ReadBySpinning(context->trio.client_socket, buffer,
                     SB_ARRAY_SIZE_INT(buffer), kSocketTimeout);

  // Tell the thread to write the B data now.
  context->write_b_signal.Put();

  // Wait until it thinks it has finished.
  context->wrote_b_signal.Take();

  // Now add the second read callback, and hope that it gets called.
  EXPECT_TRUE(SbSocketWaiterAdd(waiter, socket, context,
                                &WakeUpSocketWaiterCallback,
                                kSbSocketWaiterInterestRead, false));
}

// This test ensures that if the socket gets written to while it is not being
// waited on, and inside a callback, it will become ready immediately the next
// time it is waited on.
TEST_P(PairSbSocketWaiterWaitTest, SunnyDayAlreadyReady) {
  AlreadyReadyContext context;
  context.waiter = SbSocketWaiterCreate();
  ASSERT_TRUE(SbSocketWaiterIsValid(context.waiter));

  context.trio =
      CreateAndConnect(GetServerAddressType(), GetClientAddressType(),
                       GetPortNumberForTests(), kSocketTimeout);
  ASSERT_TRUE(SbSocketIsValid(context.trio.server_socket));

  EXPECT_TRUE(SbSocketWaiterAdd(context.waiter, context.trio.client_socket,
                                &context, &AlreadyReadySocketWaiterCallback,
                                kSbSocketWaiterInterestRead, false));

  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true, NULL,
                     AlreadyReadyEntryPoint, &context);
  EXPECT_TRUE(SbThreadIsValid(thread));
  context.wrote_a_signal.Take();

  WaitShouldNotBlock(context.waiter);

  EXPECT_TRUE(SbThreadJoin(thread, NULL));
}

TEST_F(SbSocketWaiterWaitTest, RainyDayInvalidWaiter) {
  WaitShouldNotBlock(kSbSocketWaiterInvalid);
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketWaiterWaitTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4,
                                          kSbSocketAddressTypeIpv6));
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketWaiterWaitTest,
    ::testing::Values(
        std::make_pair(kSbSocketAddressTypeIpv4, kSbSocketAddressTypeIpv4),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv6),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv4)));
#else
INSTANTIATE_TEST_CASE_P(SbSocketAddressTypes,
                        SbSocketWaiterWaitTest,
                        ::testing::Values(kSbSocketAddressTypeIpv4));
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketWaiterWaitTest,
    ::testing::Values(std::make_pair(kSbSocketAddressTypeIpv4,
                                     kSbSocketAddressTypeIpv4)));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
