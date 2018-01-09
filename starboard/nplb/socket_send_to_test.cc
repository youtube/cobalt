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

// SendTo is largely tested with ReceiveFrom, so look there for more invovled
// tests.

#include <utility>

#include "starboard/memory.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PairSbSocketSendToTest
    : public ::testing::TestWithParam<
          std::pair<SbSocketAddressType, SbSocketAddressType> > {
 public:
  SbSocketAddressType GetServerAddressType() { return GetParam().first; }
  SbSocketAddressType GetClientAddressType() { return GetParam().second; }
};

// Thread entry point to continuously write to a socket that is expected to
// be closed on another thread.
void* SendToServerSocketEntryPoint(void* trio_as_void_ptr) {
  ConnectedTrio* trio = static_cast<ConnectedTrio*>(trio_as_void_ptr);
  // The contents of this buffer are inconsequential.
  const size_t kBufSize = 1024;
  char* send_buf = new char[kBufSize];
  SbMemorySet(send_buf, 0, kBufSize);

  // Continue sending to the socket until it fails to send. It's expected that
  // SbSocketSendTo will fail when the server socket closes, but the application
  // should not terminate.
  SbTime start = SbTimeGetMonotonicNow();
  SbTime now = start;
  SbTime kTimeout = kSbTimeSecond;
  int result = 0;
  while (result >= 0 && (now - start < kTimeout)) {
    result = SbSocketSendTo(trio->server_socket, send_buf, kBufSize, NULL);
    now = SbTimeGetMonotonicNow();
  }

  delete[] send_buf;
  return NULL;
}

TEST(SbSocketSendToTest, RainyDayInvalidSocket) {
  char buf[16];
  int result = SbSocketSendTo(NULL, buf, sizeof(buf), NULL);
  EXPECT_EQ(-1, result);
}

TEST(SbSocketSendToTest, RainyDayUnconnectedSocket) {
  SbSocket socket =
      SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp);
  ASSERT_TRUE(SbSocketIsValid(socket));

  char buf[16];
  int result = SbSocketSendTo(socket, buf, sizeof(buf), NULL);
  EXPECT_EQ(-1, result);

#if SB_HAS(SOCKET_ERROR_CONNECTION_RESET_SUPPORT) || \
    SB_API_VERSION >= SB_ADDITIONAL_SOCKET_CONNECTION_ERRORS_API_VERSION
  EXPECT_SB_SOCKET_ERROR_IN(SbSocketGetLastError(socket),
                            kSbSocketErrorConnectionReset,
                            kSbSocketErrorFailed);
#else
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketGetLastError(socket));
#endif  // SB_HAS(SOCKET_ERROR_CONNECTION_RESET_SUPPORT) ||
        // SB_API_VERSION >= SB_ADDITIONAL_SOCKET_CONNECTION_ERRORS_API_VERSION

  EXPECT_TRUE(SbSocketDestroy(socket));
}

TEST_P(PairSbSocketSendToTest, RainyDaySendToClosedSocket) {
  ConnectedTrio trio =
      CreateAndConnect(GetServerAddressType(), GetClientAddressType(),
                       GetPortNumberForTests(), kSocketTimeout);
  EXPECT_NE(trio.client_socket, kSbSocketInvalid);
  EXPECT_NE(trio.server_socket, kSbSocketInvalid);
  EXPECT_NE(trio.listen_socket, kSbSocketInvalid);

  // We don't need the listen socket, so close it.
  EXPECT_TRUE(SbSocketDestroy(trio.listen_socket));

  // Start a thread to write to the client socket.
  const bool kJoinable = true;
  SbThread send_thread = SbThreadCreate(
      0, kSbThreadNoPriority, kSbThreadNoAffinity, kJoinable, "SendToTest",
      SendToServerSocketEntryPoint, static_cast<void*>(&trio));

  // Close the client, which should cause writes to the server socket to fail.
  EXPECT_TRUE(SbSocketDestroy(trio.client_socket));

  // Wait for the thread to exit and check the last socket error.
  void* thread_result;
  EXPECT_TRUE(SbThreadJoin(send_thread, &thread_result));

#if SB_HAS(SOCKET_ERROR_CONNECTION_RESET_SUPPORT) || \
    SB_API_VERSION >= SB_ADDITIONAL_SOCKET_CONNECTION_ERRORS_API_VERSION
  EXPECT_SB_SOCKET_ERROR_IN(SbSocketGetLastError(trio.server_socket),
                            kSbSocketErrorConnectionReset,
                            kSbSocketErrorFailed);
#else
  EXPECT_SB_SOCKET_ERROR_IS_ERROR(SbSocketGetLastError(trio.server_socket));
#endif  // SB_HAS(SOCKET_ERROR_CONNECTION_RESET_SUPPORT) ||
        // SB_API_VERSION >= SB_ADDITIONAL_SOCKET_CONNECTION_ERRORS_API_VERSION

  // Clean up the server socket.
  EXPECT_TRUE(SbSocketDestroy(trio.server_socket));
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketSendToTest,
    ::testing::Values(
        std::make_pair(kSbSocketAddressTypeIpv4, kSbSocketAddressTypeIpv4),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv6),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv4)));
#else
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketSendToTest,
    ::testing::Values(std::make_pair(kSbSocketAddressTypeIpv4,
                                     kSbSocketAddressTypeIpv4)));
#endif
}  // namespace
}  // namespace nplb
}  // namespace starboard
