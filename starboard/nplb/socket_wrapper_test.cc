// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/nplb/socket_helpers.h"
#include "starboard/socket.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PairSbSocketWrapperTest
    : public ::testing::TestWithParam<
          std::pair<SbSocketAddressType, SbSocketAddressType> > {
 public:
  SbSocketAddressType GetServerAddressType() { return GetParam().first; }
  SbSocketAddressType GetClientAddressType() { return GetParam().second; }
};

TEST_P(PairSbSocketWrapperTest, SunnyDay) {
  const int kBufSize = 256 * 1024;
  const int kSockBufSize = kBufSize / 8;

  scoped_ptr<ConnectedTrioWrapped> trio =
      CreateAndConnectWrapped(GetServerAddressType(), GetClientAddressType(),
                              GetPortNumberForTests(), kSocketTimeout);
  ASSERT_TRUE(trio);
  ASSERT_TRUE(trio->server_socket);
  ASSERT_TRUE(trio->server_socket->IsValid());

  // Let's set the buffers small to create partial reads and writes.
  trio->client_socket->SetReceiveBufferSize(kSockBufSize);
  trio->server_socket->SetReceiveBufferSize(kSockBufSize);
  trio->client_socket->SetSendBufferSize(kSockBufSize);
  trio->server_socket->SetSendBufferSize(kSockBufSize);

  // Create the buffers and fill the send buffer with a pattern, the receive
  // buffer with zeros.
  scoped_array<char> pattern(new char[kBufSize]);
  scoped_array<char> send_buf(new char[kBufSize]);
  scoped_array<char> receive_buf(new char[kBufSize]);
  for (int i = 0; i < kBufSize; ++i) {
    pattern[i] = static_cast<char>(i);
    send_buf[i] = static_cast<char>(i);
    receive_buf[i] = 0;
  }

  // Send from server to client and verify the pattern.
  int transferred =
      Transfer(trio->client_socket.get(), receive_buf.get(),
               trio->server_socket.get(), send_buf.get(), kBufSize);
  EXPECT_EQ(kBufSize, transferred);
  for (int i = 0; i < kBufSize; ++i) {
    EXPECT_EQ(pattern[i], send_buf[i]) << "Position " << i;
    EXPECT_EQ(pattern[i], receive_buf[i]) << "Position " << i;
  }

  // Try the other way now, first clearing the target buffer again.
  for (int i = 0; i < kBufSize; ++i) {
    receive_buf[i] = 0;
  }
  transferred = Transfer(trio->server_socket.get(), receive_buf.get(),
                         trio->client_socket.get(), send_buf.get(), kBufSize);
  EXPECT_EQ(kBufSize, transferred);
  for (int i = 0; i < kBufSize; ++i) {
    EXPECT_EQ(pattern[i], send_buf[i]) << "Position " << i;
    EXPECT_EQ(pattern[i], receive_buf[i]) << "Position " << i;
  }
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketWrapperTest,
    ::testing::Values(
        std::make_pair(kSbSocketAddressTypeIpv4, kSbSocketAddressTypeIpv4),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv6),
        std::make_pair(kSbSocketAddressTypeIpv6, kSbSocketAddressTypeIpv4)));
#else
INSTANTIATE_TEST_CASE_P(
    SbSocketAddressTypes,
    PairSbSocketWrapperTest,
    ::testing::Values(std::make_pair(kSbSocketAddressTypeIpv4,
                                     kSbSocketAddressTypeIpv4)));
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
