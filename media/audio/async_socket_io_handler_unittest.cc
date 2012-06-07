// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/async_socket_io_handler.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kAsyncSocketIoTestString[] = "Hello, AsyncSocketIoHandler";
const size_t kAsyncSocketIoTestStringLength =
    arraysize(kAsyncSocketIoTestString);

class TestSocketReader {
 public:
  TestSocketReader(base::CancelableSyncSocket* socket, bool quit_on_read)
      : socket_(socket), buffer_(), quit_on_read_(quit_on_read) {
    io_handler.Initialize(socket_->handle());
  }
  ~TestSocketReader() {}

  bool IssueRead() {
    return io_handler.Read(&buffer_[0], sizeof(buffer_),
                           base::Bind(&TestSocketReader::OnRead,
                                      base::Unretained(this)));
  }

  const char* buffer() const { return &buffer_[0]; }

 private:
  void OnRead(int bytes_read) {
    EXPECT_GT(bytes_read, 0);
    if (quit_on_read_)
      MessageLoop::current()->Quit();
  }

  media::AsyncSocketIoHandler io_handler;
  base::CancelableSyncSocket* socket_;  // Ownership lies outside the class.
  char buffer_[kAsyncSocketIoTestStringLength];
  bool quit_on_read_;
};

}  // end namespace.

// Tests doing a pending read from a socket and use an IO handler to get
// notified of data.
TEST(AsyncSocketIoHandlerTest, AsynchronousReadWithMessageLoop) {
  MessageLoopForIO loop;

  base::CancelableSyncSocket pair[2];
  ASSERT_TRUE(base::CancelableSyncSocket::CreatePair(&pair[0], &pair[1]));

  TestSocketReader reader(&pair[0], true);
  EXPECT_TRUE(reader.IssueRead());

  pair[1].Send(kAsyncSocketIoTestString, kAsyncSocketIoTestStringLength);
  MessageLoop::current()->Run();
  EXPECT_EQ(strcmp(reader.buffer(), kAsyncSocketIoTestString), 0);
}

// Tests doing a read from a socket when we know that there is data in the
// socket.  Here we want to make sure that any async 'can read' notifications
// won't trip us off and that the synchronous case works as well.
TEST(AsyncSocketIoHandlerTest, SynchronousReadWithMessageLoop) {
  MessageLoopForIO loop;

  base::CancelableSyncSocket pair[2];
  ASSERT_TRUE(base::CancelableSyncSocket::CreatePair(&pair[0], &pair[1]));

  TestSocketReader reader(&pair[0], false);

  pair[1].Send(kAsyncSocketIoTestString, kAsyncSocketIoTestStringLength);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(),
      base::TimeDelta::FromMilliseconds(100));
  MessageLoop::current()->Run();

  EXPECT_TRUE(reader.IssueRead());
  EXPECT_EQ(strcmp(reader.buffer(), kAsyncSocketIoTestString), 0);
}
