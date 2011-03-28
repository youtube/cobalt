// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy_impl.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"


class MessageLoopProxyImplTest : public testing::Test {
 public:
  void Release() const {
    AssertOnIOThread();
    Quit();
  }

  void Quit() const {
    loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

  void AssertOnIOThread() const {
    ASSERT_TRUE(io_thread_->message_loop_proxy()->BelongsToCurrentThread());
  }

  void AssertOnFileThread() const {
    ASSERT_TRUE(file_thread_->message_loop_proxy()->BelongsToCurrentThread());
  }

 protected:
  virtual void SetUp() {
    io_thread_.reset(new base::Thread("MessageLoopProxyImplTest_IO"));
    file_thread_.reset(new base::Thread("MessageLoopProxyImplTest_File"));
    io_thread_->Start();
    file_thread_->Start();
  }

  virtual void TearDown() {
    io_thread_->Stop();
    file_thread_->Stop();
  }

  static void BasicFunction(MessageLoopProxyImplTest* test) {
    test->AssertOnFileThread();
    test->Quit();
  }

  class DummyTask : public Task {
   public:
    explicit DummyTask(bool* deleted) : deleted_(deleted) { }
    ~DummyTask() {
      *deleted_ = true;
    }

    void Run() {
      FAIL();
    }

   private:
    bool* deleted_;
  };

  class DeletedOnFile {
   public:
    explicit DeletedOnFile(MessageLoopProxyImplTest* test) : test_(test) {}

    ~DeletedOnFile() {
      test_->AssertOnFileThread();
      test_->Quit();
    }

   private:
    MessageLoopProxyImplTest* test_;
  };

  scoped_ptr<base::Thread> io_thread_;
  scoped_ptr<base::Thread> file_thread_;

 private:
  mutable MessageLoop loop_;
};


TEST_F(MessageLoopProxyImplTest, PostTask) {
  EXPECT_TRUE(file_thread_->message_loop_proxy()->PostTask(
      FROM_HERE, NewRunnableFunction(&BasicFunction, this)));
  MessageLoop::current()->Run();
}

TEST_F(MessageLoopProxyImplTest, Release) {
  EXPECT_TRUE(io_thread_->message_loop_proxy()->ReleaseSoon(FROM_HERE, this));
  MessageLoop::current()->Run();
}

TEST_F(MessageLoopProxyImplTest, Delete) {
  DeletedOnFile* deleted_on_file = new DeletedOnFile(this);
  EXPECT_TRUE(file_thread_->message_loop_proxy()->DeleteSoon(
      FROM_HERE, deleted_on_file));
  MessageLoop::current()->Run();
}

TEST_F(MessageLoopProxyImplTest, PostTaskAfterThreadExits) {
  scoped_ptr<base::Thread> test_thread(
      new base::Thread("MessageLoopProxyImplTest_Dummy"));
  test_thread->Start();
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
      test_thread->message_loop_proxy();
  test_thread->Stop();

  bool deleted = false;
  bool ret = message_loop_proxy->PostTask(
      FROM_HERE, new DummyTask(&deleted));
  EXPECT_FALSE(ret);
  EXPECT_TRUE(deleted);
}

TEST_F(MessageLoopProxyImplTest, PostTaskAfterThreadIsDeleted) {
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy;
  {
    scoped_ptr<base::Thread> test_thread(
        new base::Thread("MessageLoopProxyImplTest_Dummy"));
    test_thread->Start();
    message_loop_proxy = test_thread->message_loop_proxy();
  }
  bool deleted = false;
  bool ret = message_loop_proxy->PostTask(FROM_HERE, new DummyTask(&deleted));
  EXPECT_FALSE(ret);
  EXPECT_TRUE(deleted);
}

