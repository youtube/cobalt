// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/worker_pool.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "base/threading/thread_checker_impl.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

typedef PlatformTest WorkerPoolTest;

namespace base {

namespace {

class PostTaskAndReplyTester
    : public base::RefCountedThreadSafe<PostTaskAndReplyTester> {
 public:
  PostTaskAndReplyTester() : finished_(false), test_event_(false, false) {
  }

  void RunTest() {
    ASSERT_TRUE(thread_checker_.CalledOnValidThread());
    WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&PostTaskAndReplyTester::OnWorkerThread, this),
      base::Bind(&PostTaskAndReplyTester::OnOriginalThread, this),
      false);

    test_event_.Wait();
  }

  void OnWorkerThread() {
    // We're not on the original thread.
    EXPECT_FALSE(thread_checker_.CalledOnValidThread());

    test_event_.Signal();
  }

  void OnOriginalThread() {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    finished_ = true;
  }

  bool finished() const {
    return finished_;
  }

 private:
  bool finished_;
  WaitableEvent test_event_;

  // The Impl version performs its checks even in release builds.
  ThreadCheckerImpl thread_checker_;
};

}  // namespace

TEST_F(WorkerPoolTest, PostTask) {
  WaitableEvent test_event(false, false);
  WaitableEvent long_test_event(false, false);

  WorkerPool::PostTask(FROM_HERE,
                       base::Bind(&WaitableEvent::Signal,
                                  base::Unretained(&test_event)),
                       false);
  WorkerPool::PostTask(FROM_HERE,
                       base::Bind(&WaitableEvent::Signal,
                                  base::Unretained(&long_test_event)),
                       true);

  test_event.Wait();
  long_test_event.Wait();
}

TEST_F(WorkerPoolTest, PostTaskAndReply) {
  MessageLoop message_loop;
  scoped_refptr<PostTaskAndReplyTester> tester(new PostTaskAndReplyTester());
  tester->RunTest();

  const TimeDelta kMaxDuration =
      TimeDelta::FromMilliseconds(TestTimeouts::tiny_timeout_ms());
  TimeTicks start = TimeTicks::Now();
  while (!tester->finished() && TimeTicks::Now() - start < kMaxDuration) {
    MessageLoop::current()->RunAllPending();
  }
  EXPECT_TRUE(tester->finished());
}

}  // namespace base
