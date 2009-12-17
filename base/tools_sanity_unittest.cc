// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// We use caps here just to ensure that the method name doesn't interfere with
// the wildcarded suppressions.
class TOOLS_SANITY_TEST_CONCURRENT_THREAD : public PlatformThread::Delegate {
 public:
  explicit TOOLS_SANITY_TEST_CONCURRENT_THREAD(bool *value) : value_(value) {}
  ~TOOLS_SANITY_TEST_CONCURRENT_THREAD() {}
  void ThreadMain() {
    *value_ = true;
  }
 private:
  bool* value_;
};

}

// A memory leak detector should report an error in this test.
TEST(ToolsSanityTest, MemoryLeak) {
  int *leak = new int[256];  // Leak some memory intentionally.
  leak[4] = 1;  // Make sure the allocated memory is used.
}

// A data race detector should report an error in this test.
TEST(ToolsSanityTest, DataRace) {
  bool shared = false;
  PlatformThreadHandle a;
  PlatformThreadHandle b;
  PlatformThread::Delegate *thread1 =
      new TOOLS_SANITY_TEST_CONCURRENT_THREAD(&shared);
  PlatformThread::Delegate *thread2 =
      new TOOLS_SANITY_TEST_CONCURRENT_THREAD(&shared);

  PlatformThread::Create(0, thread1, &a);
  PlatformThread::Create(0, thread2, &b);
  PlatformThread::Join(a);
  PlatformThread::Join(b);
  EXPECT_TRUE(shared);
  delete thread1;
  delete thread2;
}

