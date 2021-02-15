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

#include "starboard/common/socket.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PairSbSocketReceiveFromTest
    : public ::testing::TestWithParam<
          std::pair<SbSocketAddressType, SbSocketAddressType> > {
 public:
  SbSocketAddressType GetServerAddressType() { return GetParam().first; }
  SbSocketAddressType GetClientAddressType() { return GetParam().second; }
};

// Transfers data between the two connected local sockets, spinning until |size|
// has been transferred, or an error occurs.
int Transfer(SbSocket receive_socket,
             char* out_data,
             SbSocket send_socket,
             const char* send_data,
             int size) {
  int send_total = 0;
  int receive_total = 0;
  while (receive_total < size) {
    if (send_total < size) {
      int bytes_sent = SbSocketSendTo(send_socket, send_data + send_total,
                                      size - send_total, NULL);
      if (bytes_sent < 0) {
        if (SbSocketGetLastError(send_socket) != kSbSocketPending) {
          return -1;
        }
        bytes_sent = 0;
      }

      send_total += bytes_sent;
    }

    int bytes_received = SbSocketReceiveFrom(
        receive_socket, out_data + receive_total, size - receive_total, NULL);
    if (bytes_received < 0) {
      if (SbSocketGetLastError(receive_socket) != kSbSocketPending) {
        return -1;
      }
      bytes_received = 0;
    }

    receive_total += bytes_received;
  }

  return size;
}

TEST_P(PairSbSocketReceiveFromTest, SunnyDay) {
  const int kBufSize = 256 * 1024;
  const int kSockBufSize = kBufSize / 8;

  ConnectedTrio trio =
      CreateAndConnect(GetServerAddressType(), GetClientAddressType(),
                       GetPortNumberForTests(), kSocketTimeout);
  ASSERT_TRUE(SbSocketIsValid(trio.server_socket));

  // Let's set the buffers small to create partial reads and writes.
  SbSocketSetReceiveBufferSize(trio.client_socket, kSockBufSize);
  SbSocketSetReceiveBufferSize(trio.server_socket, kSockBufSize);
  SbSocketSetSendBufferSize(trio.client_socket, kSockBufSize);
  SbSocketSetSendBufferSize(trio.server_socket, kSockBufSize);

  // Create the buffers and fill the send buffer with a pattern, the receive
  // buffer with zeros.
  char* send_buf = new char[kBufSize];
  char* receive_buf = new char[kBufSize];
  for (int i = 0; i < kBufSize; ++i) {
    send_buf[i] = static_cast<char>(i);
    receive_buf[i] = 0;
  }

  // Send from server to client and verify the pattern.
  int transferred = Transfer(trio.client_socket, receive_buf,
                             trio.server_socket, send_buf, kBufSize);
  EXPECT_EQ(kBufSize, transferred);
  for (int i = 0; i < kBufSize; ++i) {
    EXPECT_EQ(send_buf[i], receive_buf[i]) << "Position " << i;
  }

  // Try the other way now, first clearing the target buffer again.
  for (int i = 0; i < kBufSize; ++i) {
    receive_buf[i] = 0;
  }
  transferred = Transfer(trio.server_socket, receive_buf, trio.client_socket,
                         send_buf, kBufSize);
  EXPECT_EQ(kBufSize, transferred);
  for (int i = 0; i < kBufSize; ++i) {
    EXPECT_EQ(send_buf[i], receive_buf[i]) << "Position " << i;
  }

  EXPECT_TRUE(SbSocketDestroy(trio.server_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.client_socket));
  EXPECT_TRUE(SbSocketDestroy(trio.listen_socket));
  delete[] send_buf;
  delete[] receive_buf;
}

TEST(SbSocketReceiveFromTest, RainyDayInvalidSocket) {
  char buf[16];
  int result = SbSocketReceiveFrom(NULL, buf, sizeof(buf), NULL);
  EXPECT_EQ(-1, result);
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketReceiveFromTest,
    ::testing::Values(
        std::make_pair(kSbSocketAddressTypeIpv4, kSbSocketAddressTypeIpv4),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv6),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv4)));
#else
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketReceiveFromTest,
    ::testing::Values(std::make_pair(kSbSocketAddressTypeIpv4,
                                     kSbSocketAddressTypeIpv4)));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
