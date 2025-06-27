// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

// The following scenarios are not reliably testable for socketpair():
// - Shall fail with ENFILE if no more file descriptors are available for the
//   system. This would be difficult to trigger from a user-space test without
//   impacting the system.
// - Shall fail with EOPNOTSUPP if the specified protocol does not permit
//   creation of socket pairs. It's not clear how to reproduce this error case
//   in a portable way; for example, Linux systems generally don't support
//   socket pairs for AF_INET sockets but some systems apparently do.

#include <fcntl.h>
#include <sys/socket.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const char kRecord1[] = "Record-1";
const char kRecord2[] = "Record-2";
const char kRecord3[] = "Record-3";

const ssize_t kRecordSize = sizeof(kRecord1);
static_assert(sizeof(kRecord1) == sizeof(kRecord2) &&
                  sizeof(kRecord2) == sizeof(kRecord3),
              "The test records are not of equal size");

void VerifyTransmission(int write_socket,
                        int read_socket,
                        const char* record,
                        const ssize_t record_size) {
  std::vector<char> buffer(record_size, 0);
  EXPECT_EQ(write(write_socket, record, record_size), record_size);
  EXPECT_EQ(read(read_socket, buffer.data(), buffer.size()), record_size);
  EXPECT_STREQ(buffer.data(), record);
}

TEST(PosixSocketpairTest, StreamTypeSocketsProvideBidrectionalTransmission) {
  int sockets[2];

  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

  int client = sockets[0];
  int server = sockets[1];
  VerifyTransmission(client, server, kRecord1, kRecordSize);
  VerifyTransmission(server, client, kRecord2, kRecordSize);

  EXPECT_EQ(close(client), 0);
  EXPECT_EQ(close(server), 0);
}

TEST(PosixSocketpairTest, SeqPacketTypeSocketsProvideBidrectionalTransmission) {
  int sockets[2];

  ASSERT_EQ(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets), 0);

  int client = sockets[0];
  int server = sockets[1];
  VerifyTransmission(client, server, kRecord1, kRecordSize);
  VerifyTransmission(server, client, kRecord2, kRecordSize);

  EXPECT_EQ(close(client), 0);
  EXPECT_EQ(close(server), 0);
}

TEST(PosixSocketpairTest,
     SeqPacketTypeIONeverTransfersPartOfMoreThanOneRecord) {
  int sockets[2];

  ASSERT_EQ(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets), 0);

  EXPECT_EQ(write(sockets[0], kRecord1, kRecordSize), kRecordSize);
  EXPECT_EQ(write(sockets[0], kRecord2, kRecordSize), kRecordSize);
  EXPECT_EQ(write(sockets[0], kRecord3, kRecordSize), kRecordSize);

  std::vector<char> buffer(kRecordSize * 3, 0);  // Could hold all records
  ssize_t bytes_read = read(sockets[1], buffer.data(), buffer.size());
  // We assert, rather than expect, to avoid blocking on subsequent reads.
  ASSERT_EQ(bytes_read, kRecordSize);
  ASSERT_STREQ(buffer.data(), kRecord1);

  bytes_read = read(sockets[1], buffer.data(), buffer.size());
  ASSERT_EQ(bytes_read, kRecordSize);
  ASSERT_STREQ(buffer.data(), kRecord2);

  bytes_read = read(sockets[1], buffer.data(), buffer.size());
  ASSERT_EQ(bytes_read, kRecordSize);
  ASSERT_STREQ(buffer.data(), kRecord3);

  EXPECT_EQ(close(sockets[0]), 0);
  EXPECT_EQ(close(sockets[1]), 0);
}

TEST(PosixSocketpairTest, SeqPacketTypeExcessBytesAreDiscardedWhenReceived) {
  int sockets[2];

  ASSERT_EQ(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets), 0);

  // Given a written record of five bytes, including the null terminator.
  const char record_1[] = "abcd";
  ssize_t record_1_size = sizeof(record_1);
  ASSERT_EQ(record_1_size, 5);
  EXPECT_EQ(write(sockets[0], record_1, record_1_size), record_1_size);

  // When the subsequent read is for only two bytes.
  std::vector<char> buffer(2, 0);
  ssize_t bytes_read = recv(sockets[1], buffer.data(), buffer.size(), 0);
  ASSERT_EQ(bytes_read, 2);
  ASSERT_EQ(buffer.at(0), 'a');
  ASSERT_EQ(buffer.at(1), 'b');

  // And a second record is then written.
  const char record_2[] = "ef";
  ssize_t record_2_size = sizeof(record_2);
  EXPECT_EQ(write(sockets[0], record_2, record_2_size), record_2_size);

  // Then the remaining two bytes from the initial record are discarded.
  bytes_read = recv(sockets[1], buffer.data(), buffer.size(), 0);
  ASSERT_EQ(bytes_read, 2);
  ASSERT_EQ(buffer.at(0), 'e');
  ASSERT_EQ(buffer.at(1), 'f');

  EXPECT_EQ(close(sockets[0]), 0);
  EXPECT_EQ(close(sockets[1]), 0);
}

TEST(PosixSocketpairTest, StreamTypeExcessBytesAreNotDiscardedWhenReceived) {
  int sockets[2];

  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

  /// Given a written record of five bytes, including the null terminator.
  const char record_1[] = "abcd";
  ssize_t record_1_size = sizeof(record_1);
  ASSERT_EQ(record_1_size, 5);
  EXPECT_EQ(write(sockets[0], record_1, record_1_size), record_1_size);

  // When the subsequent read is for only two bytes.
  std::vector<char> buffer(2, 0);
  ssize_t bytes_read = recv(sockets[1], buffer.data(), buffer.size(), 0);
  ASSERT_EQ(bytes_read, 2);
  ASSERT_EQ(buffer.at(0), 'a');
  ASSERT_EQ(buffer.at(1), 'b');

  // And a second record is then written.
  const char record_2[] = "ef";
  ssize_t record_2_size = sizeof(record_2);
  EXPECT_EQ(write(sockets[0], record_2, record_2_size), record_2_size);

  // Then the remaining two bytes from the initial record are NOT discarded.
  bytes_read = recv(sockets[1], buffer.data(), buffer.size(), 0);
  ASSERT_EQ(bytes_read, 2);
  ASSERT_EQ(buffer.at(0), 'c');
  ASSERT_EQ(buffer.at(1), 'd');

  EXPECT_EQ(close(sockets[0]), 0);
  EXPECT_EQ(close(sockets[1]), 0);
}

TEST(PosixSocketpairTest, SocketpairWithCloseOnExecSetsThisFlagOnBothSockets) {
  int sockets[2];

  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets), 0);

  int descriptor_flags = fcntl(sockets[0], F_GETFD);
  EXPECT_NE(descriptor_flags, -1);
  EXPECT_TRUE(descriptor_flags & FD_CLOEXEC);

  descriptor_flags = fcntl(sockets[1], F_GETFD);
  EXPECT_NE(descriptor_flags, -1);
  EXPECT_TRUE(descriptor_flags & FD_CLOEXEC);

  close(sockets[0]);
  close(sockets[1]);
}

TEST(PosixSocketpairTest, SocketpairWithNonblockSetsThisFlagOnBothSockets) {
  int sockets[2];

  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, sockets), 0);

  int file_status_flags = fcntl(sockets[0], F_GETFL);
  EXPECT_NE(file_status_flags, -1);
  EXPECT_TRUE(file_status_flags & O_NONBLOCK);

  file_status_flags = fcntl(sockets[1], F_GETFL);
  EXPECT_NE(file_status_flags, -1);
  EXPECT_TRUE(file_status_flags & O_NONBLOCK);

  close(sockets[0]);
  close(sockets[1]);
}

TEST(PosixSocketpairTest,
     SocketpairWithMultipleFlagsSetsTheseFlagsOnBothSockets) {
  int sockets[2];

  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0,
                       sockets),
            0);

  int descriptor_flags = fcntl(sockets[0], F_GETFD);
  EXPECT_NE(descriptor_flags, -1);
  EXPECT_TRUE(descriptor_flags & FD_CLOEXEC);

  descriptor_flags = fcntl(sockets[1], F_GETFD);
  EXPECT_NE(descriptor_flags, -1);
  EXPECT_TRUE(descriptor_flags & FD_CLOEXEC);

  int file_status_flags = fcntl(sockets[0], F_GETFL);
  EXPECT_NE(file_status_flags, -1);
  EXPECT_TRUE(file_status_flags & O_NONBLOCK);

  file_status_flags = fcntl(sockets[1], F_GETFL);
  EXPECT_NE(file_status_flags, -1);
  EXPECT_TRUE(file_status_flags & O_NONBLOCK);

  close(sockets[0]);
  close(sockets[1]);
}

TEST(PosixSocketpairTest, SocketpairWithInvalidDomainFails) {
  int sockets[2];

  EXPECT_EQ(socketpair(-1, SOCK_STREAM, 0, sockets), -1);
  EXPECT_EQ(errno, EAFNOSUPPORT);

  close(sockets[0]);
  close(sockets[1]);
}

TEST(PosixSocketpairTest, SocketpairWithInvalidTypeFails) {
  int sockets[2];

  EXPECT_EQ(socketpair(AF_UNIX, -1, 0, sockets), -1);
  // The POSIX spec seems to require EPROTOTYPE here but that behavior is not
  // consistently implemented; some Linux systems choose EINVAL, which is not a
  // documented error for socketpair. So, for portability, we don't make any
  // expectation about the specific error.

  close(sockets[0]);
  close(sockets[1]);
}

TEST(PosixSocketpairTest, SocketpairWithInvalidProtocolFails) {
  int sockets[2];

  EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, -1, sockets), -1);
  EXPECT_EQ(errno, EPROTONOSUPPORT);

  close(sockets[0]);
  close(sockets[1]);
}

TEST(PosixSocketpairTest,
     DISABLED_SocketpairReturnsEmfileIfTooManyFileDescriptorsOpen) {
  // TODO: b/412648662 - If/when getrlimit() and setrimit() are added to the
  // hermetic build, use them to drop the maximum value that the system can
  // assign to new file descriptors for this process to just above the current
  // usage in order to easily force this scenario.
  FAIL() << "This test requires the ability to manipulate process resource "
         << "limits (rlimit) to reliably trigger EMFILE, which is not "
         << "currently available in this test environment.";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
