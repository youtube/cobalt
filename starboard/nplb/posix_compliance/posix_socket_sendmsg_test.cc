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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixSendmsgTest, SunnyDay) {
  int sockets[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));

  char message[] = "Hello, world!";
  struct iovec iov[1];
  iov[0].iov_base = message;
  iov[0].iov_len = strlen(message);

  struct msghdr msg = {};
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  ssize_t bytes_sent = sendmsg(sockets[0], &msg, 0);
  EXPECT_EQ(strlen(message), static_cast<size_t>(bytes_sent));

  char buffer[strlen(message) + 1];
  ssize_t bytes_received = read(sockets[1], buffer, sizeof(buffer));
  EXPECT_EQ(bytes_sent, bytes_received);
  EXPECT_EQ(0, strncmp(message, buffer, bytes_received));

  EXPECT_EQ(0, close(sockets[0]));
  EXPECT_EQ(0, close(sockets[1]));
}

TEST(PosixSendmsgTest, SunnyDayUdpWithRecvmsg) {
  int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
  ASSERT_NE(-1, server_fd);

  struct sockaddr_in server_addr = {};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  server_addr.sin_port = 0;  // Bind to any available port
  ASSERT_EQ(
      0, bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)));

  socklen_t server_addr_len = sizeof(server_addr);
  ASSERT_EQ(0, getsockname(server_fd, (struct sockaddr*)&server_addr,
                           &server_addr_len));

  int client_fd = socket(AF_INET, SOCK_DGRAM, 0);
  ASSERT_NE(-1, client_fd);

  struct sockaddr_in client_addr = {};
  client_addr.sin_family = AF_INET;
  client_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  client_addr.sin_port = 0;  // Bind to any available port
  ASSERT_EQ(
      0, bind(client_fd, (struct sockaddr*)&client_addr, sizeof(client_addr)));

  socklen_t client_addr_len = sizeof(client_addr);
  ASSERT_EQ(0, getsockname(client_fd, (struct sockaddr*)&client_addr,
                           &client_addr_len));

  char message[] = "Hello, UDP!";
  struct iovec iov[1];
  iov[0].iov_base = message;
  iov[0].iov_len = strlen(message);

  struct msghdr send_msg = {};
  send_msg.msg_name = &server_addr;
  send_msg.msg_namelen = sizeof(server_addr);
  send_msg.msg_iov = iov;
  send_msg.msg_iovlen = 1;

  ssize_t bytes_sent = sendmsg(client_fd, &send_msg, 0);
  EXPECT_EQ(strlen(message), static_cast<size_t>(bytes_sent));

  char buffer[strlen(message) + 1];
  struct iovec recv_iov[1];
  recv_iov[0].iov_base = buffer;
  recv_iov[0].iov_len = sizeof(buffer);

  struct sockaddr_in recv_client_addr = {};
  struct msghdr recv_msg = {};
  recv_msg.msg_name = &recv_client_addr;
  recv_msg.msg_namelen = sizeof(recv_client_addr);
  recv_msg.msg_iov = recv_iov;
  recv_msg.msg_iovlen = 1;

  ssize_t bytes_received = recvmsg(server_fd, &recv_msg, 0);
  EXPECT_EQ(bytes_sent, bytes_received);
  EXPECT_EQ(0, strncmp(message, buffer, bytes_received));
  EXPECT_EQ(client_addr.sin_port, recv_client_addr.sin_port);
  EXPECT_EQ(client_addr.sin_addr.s_addr, recv_client_addr.sin_addr.s_addr);
  EXPECT_EQ(client_addr.sin_family, recv_client_addr.sin_family);
  EXPECT_EQ(sizeof(client_addr), recv_msg.msg_namelen);

  EXPECT_EQ(0, close(server_fd));
  EXPECT_EQ(0, close(client_fd));
}

TEST(PosixSendmsgTest, SunnyDayWithAncillaryData) {
  int sockets[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));

  int pipe_fds[2];
  ASSERT_EQ(0, pipe(pipe_fds));

  char message[] = "Here is a file descriptor!";
  {
    struct iovec iov[1];
    iov[0].iov_base = message;
    iov[0].iov_len = strlen(message);

    struct msghdr msg = {};
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    union {
      char buf[CMSG_SPACE(sizeof(int))];
      struct cmsghdr align;
    } control_un;
    memset(control_un.buf, 0, sizeof(control_un.buf));
    msg.msg_control = control_un.buf;
    msg.msg_controllen = sizeof(control_un.buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &pipe_fds[0], sizeof(int));

    ssize_t bytes_sent = sendmsg(sockets[0], &msg, 0);
    ASSERT_NE(-1, bytes_sent) << "sendmsg failed with errno: " << errno;
    EXPECT_EQ(strlen(message), static_cast<size_t>(bytes_sent));
    // Close the sending end of the pipe to avoid deadlock.
    EXPECT_EQ(0, close(pipe_fds[0]));
  }

  {
    char buffer[strlen(message) + 1];
    struct iovec iov[1];
    iov[0].iov_base = buffer;
    iov[0].iov_len = sizeof(buffer);

    struct msghdr msg = {};
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    union {
      char buf[CMSG_SPACE(sizeof(int))];
      struct cmsghdr align;
    } control_un;
    memset(control_un.buf, 0, sizeof(control_un.buf));
    msg.msg_control = control_un.buf;
    msg.msg_controllen = sizeof(control_un.buf);

    ssize_t bytes_received = recvmsg(sockets[1], &msg, 0);
    ASSERT_NE(-1, bytes_received) << "recvmsg failed with errno: " << errno;

    ASSERT_FALSE(msg.msg_flags & MSG_CTRUNC)
        << "Control message was truncated.";

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    ASSERT_NE(nullptr, cmsg);
    EXPECT_EQ(SOL_SOCKET, cmsg->cmsg_level);
    EXPECT_EQ(SCM_RIGHTS, cmsg->cmsg_type);
    EXPECT_EQ(CMSG_LEN(sizeof(int)), cmsg->cmsg_len);

    int received_fd = -1;
    memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
    ASSERT_GE(received_fd, 0);

    const char* pipe_message = "Hello from the pipe!";
    ssize_t written_bytes =
        write(pipe_fds[1], pipe_message, strlen(pipe_message));
    EXPECT_EQ(strlen(pipe_message), static_cast<size_t>(written_bytes));

    char pipe_buffer[64] = {0};
    ssize_t pipe_bytes_read =
        read(received_fd, pipe_buffer, sizeof(pipe_buffer) - 1);
    EXPECT_EQ(strlen(pipe_message), static_cast<size_t>(pipe_bytes_read));
    EXPECT_EQ(0, strncmp(pipe_message, pipe_buffer, pipe_bytes_read));

    close(received_fd);
  }

  EXPECT_EQ(0, close(sockets[0]));
  EXPECT_EQ(0, close(sockets[1]));
  EXPECT_EQ(0, close(pipe_fds[1]));
}

TEST(PosixSendmsgTest, RainyDayInvalidSocket) {
  char message[] = "Hello, world!";
  struct iovec iov[1];
  iov[0].iov_base = message;
  iov[0].iov_len = strlen(message);

  struct msghdr msg = {};
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  ssize_t bytes_sent = sendmsg(-1, &msg, 0);
  EXPECT_EQ(-1, bytes_sent);
  EXPECT_EQ(EBADF, errno);
}

TEST(PosixSendmsgTest, SunnyDayTwoIovecs) {
  int sockets[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));

  char msg1[] = "Hello, ";
  char msg2[] = "world!";
  char expected_msg[sizeof(msg1) + sizeof(msg2) - 1];
  snprintf(expected_msg, sizeof(expected_msg), "%s%s", msg1, msg2);

  struct iovec iov[2];
  iov[0].iov_base = msg1;
  iov[0].iov_len = strlen(msg1);
  iov[1].iov_base = msg2;
  iov[1].iov_len = strlen(msg2);

  struct msghdr msg = {};
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;

  ssize_t bytes_sent = sendmsg(sockets[0], &msg, 0);
  EXPECT_EQ(strlen(expected_msg), static_cast<size_t>(bytes_sent));

  char buffer[strlen(expected_msg) + 1];
  ssize_t bytes_received = read(sockets[1], buffer, sizeof(buffer));
  EXPECT_EQ(bytes_sent, bytes_received);
  EXPECT_EQ(0, strncmp(expected_msg, buffer, bytes_received));

  EXPECT_EQ(0, close(sockets[0]));
  EXPECT_EQ(0, close(sockets[1]));
}

TEST(PosixSendmsgTest, SunnyDayWithMultipleAncillaryData) {
  int sockets[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));

  int pipe1_fds[2];
  ASSERT_EQ(0, pipe(pipe1_fds));
  int pipe2_fds[2];
  ASSERT_EQ(0, pipe(pipe2_fds));

  char message[] = "Here are two file descriptors!";
  {
    struct iovec iov[1];
    iov[0].iov_base = message;
    iov[0].iov_len = strlen(message);

    struct msghdr msg = {};
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    union {
      char buf[CMSG_SPACE(sizeof(int) * 2)];
      struct cmsghdr align;
    } control_un;
    memset(control_un.buf, 0, sizeof(control_un.buf));
    msg.msg_control = control_un.buf;
    msg.msg_controllen = sizeof(control_un.buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * 2);
    int fds_to_send[2] = {pipe1_fds[0], pipe2_fds[0]};
    memcpy(CMSG_DATA(cmsg), fds_to_send, sizeof(int) * 2);

    ssize_t bytes_sent = sendmsg(sockets[0], &msg, 0);
    ASSERT_NE(-1, bytes_sent) << "sendmsg failed with errno: " << errno;
    EXPECT_EQ(strlen(message), static_cast<size_t>(bytes_sent));
    // Close the sending ends of the pipes to avoid deadlock.
    EXPECT_EQ(0, close(pipe1_fds[0]));
    EXPECT_EQ(0, close(pipe2_fds[0]));
  }

  {
    char buffer[strlen(message) + 1];
    struct iovec iov[1];
    iov[0].iov_base = buffer;
    iov[0].iov_len = sizeof(buffer);

    struct msghdr msg = {};
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    union {
      char buf[CMSG_SPACE(sizeof(int) * 2)];
      struct cmsghdr align;
    } control_un;
    memset(control_un.buf, 0, sizeof(control_un.buf));
    msg.msg_control = control_un.buf;
    msg.msg_controllen = sizeof(control_un.buf);

    ssize_t bytes_received = recvmsg(sockets[1], &msg, 0);
    ASSERT_NE(-1, bytes_received) << "recvmsg failed with errno: " << errno;

    ASSERT_FALSE(msg.msg_flags & MSG_CTRUNC)
        << "Control message was truncated.";

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    ASSERT_NE(nullptr, cmsg);
    EXPECT_EQ(SOL_SOCKET, cmsg->cmsg_level);
    EXPECT_EQ(SCM_RIGHTS, cmsg->cmsg_type);
    EXPECT_EQ(CMSG_LEN(sizeof(int) * 2), cmsg->cmsg_len);

    int received_fds[2] = {-1, -1};
    memcpy(received_fds, CMSG_DATA(cmsg), sizeof(int) * 2);
    ASSERT_GE(received_fds[0], 0);
    ASSERT_GE(received_fds[1], 0);

    const char* pipe_message1 = "Hello from pipe 1!";
    ssize_t written_bytes1 =
        write(pipe1_fds[1], pipe_message1, strlen(pipe_message1));
    EXPECT_EQ(strlen(pipe_message1), static_cast<size_t>(written_bytes1));

    char pipe_buffer1[64] = {0};
    ssize_t pipe_bytes_read1 =
        read(received_fds[0], pipe_buffer1, sizeof(pipe_buffer1) - 1);
    EXPECT_EQ(strlen(pipe_message1), static_cast<size_t>(pipe_bytes_read1));
    EXPECT_EQ(0, strncmp(pipe_message1, pipe_buffer1, pipe_bytes_read1));

    const char* pipe_message2 = "Hello from pipe 2!";
    ssize_t written_bytes2 =
        write(pipe2_fds[1], pipe_message2, strlen(pipe_message2));
    EXPECT_EQ(strlen(pipe_message2), static_cast<size_t>(written_bytes2));

    char pipe_buffer2[64] = {0};
    ssize_t pipe_bytes_read2 =
        read(received_fds[1], pipe_buffer2, sizeof(pipe_buffer2) - 1);
    EXPECT_EQ(strlen(pipe_message2), static_cast<size_t>(pipe_bytes_read2));
    EXPECT_EQ(0, strncmp(pipe_message2, pipe_buffer2, pipe_bytes_read2));

    close(received_fds[0]);
    close(received_fds[1]);
  }

  EXPECT_EQ(0, close(sockets[0]));
  EXPECT_EQ(0, close(sockets[1]));
  EXPECT_EQ(0, close(pipe1_fds[1]));
  EXPECT_EQ(0, close(pipe2_fds[1]));
}

}  // namespace
}  // namespace nplb
