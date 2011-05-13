// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_libevent.h"

#include <unistd.h>

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MessagePumpLibeventTest : public testing::Test {
 public:
  MessagePumpLibeventTest()
      : ui_loop_(MessageLoop::TYPE_UI),
        io_thread_("MessagePumpLibeventTestIOThread") {}
  virtual ~MessagePumpLibeventTest() {}

  virtual void SetUp() {
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    ASSERT_TRUE(io_thread_.StartWithOptions(options));
    ASSERT_EQ(MessageLoop::TYPE_IO, io_thread_.message_loop()->type());
  }

  MessageLoop* ui_loop() { return &ui_loop_; }
  MessageLoopForIO* io_loop() const {
    return static_cast<MessageLoopForIO*>(io_thread_.message_loop());
  }

 private:
  MessageLoop ui_loop_;
  base::Thread io_thread_;
  DISALLOW_COPY_AND_ASSIGN(MessagePumpLibeventTest);
};

// Concrete implementation of base::MessagePumpLibevent::Watcher that does
// nothing useful.
class StupidWatcher : public base::MessagePumpLibevent::Watcher {
 public:
  virtual ~StupidWatcher() {}

  // base:MessagePumpLibEvent::Watcher interface
  virtual void OnFileCanReadWithoutBlocking(int fd) {}
  virtual void OnFileCanWriteWithoutBlocking(int fd) {}
};

}  // namespace

#if GTEST_HAS_DEATH_TEST

// Test to make sure that we catch calling WatchFileDescriptor off of the
// wrong thread.
TEST_F(MessagePumpLibeventTest, TestWatchingFromBadThread) {
  base::MessagePumpLibevent::FileDescriptorWatcher watcher;
  StupidWatcher delegate;

  ASSERT_DEBUG_DEATH(io_loop()->WatchFileDescriptor(
      STDOUT_FILENO, false, MessageLoopForIO::WATCH_READ, &watcher, &delegate),
      "Check failed: "
      "watch_file_descriptor_caller_checker_.CalledOnValidThread()");
}

#endif  // GTEST_HAS_DEATH_TEST
