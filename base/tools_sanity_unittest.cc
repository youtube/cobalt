// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/atomicops.h"
#include "base/message_loop.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

const base::subtle::Atomic32 kMagicValue = 42;

void ReadUninitializedValue(char *ptr) {
  if (*ptr == '\0') {
    (*ptr)++;
  } else {
    (*ptr)--;
  }
}

void ReadValueOutOfArrayBoundsLeft(char *ptr) {
  char c = ptr[-2];
  VLOG(1) << "Reading a byte out of bounds: " << c;
}

void ReadValueOutOfArrayBoundsRight(char *ptr, size_t size) {
  char c = ptr[size + 1];
  VLOG(1) << "Reading a byte out of bounds: " << c;
}

// This is harmless if you run it under Valgrind thanks to redzones.
void WriteValueOutOfArrayBoundsLeft(char *ptr) {
  ptr[-1] = kMagicValue;
}

// This is harmless if you run it under Valgrind thanks to redzones.
void WriteValueOutOfArrayBoundsRight(char *ptr, size_t size) {
  ptr[size] = kMagicValue;
}

void MakeSomeErrors(char *ptr, size_t size) {
  ReadUninitializedValue(ptr);
  ReadValueOutOfArrayBoundsLeft(ptr);
  ReadValueOutOfArrayBoundsRight(ptr, size);
  WriteValueOutOfArrayBoundsLeft(ptr);
  WriteValueOutOfArrayBoundsRight(ptr, size);
}

}  // namespace

// A memory leak detector should report an error in this test.
TEST(ToolsSanityTest, MemoryLeak) {
  int *leak = new int[256];  // Leak some memory intentionally.
  leak[4] = 1;  // Make sure the allocated memory is used.
}

TEST(ToolsSanityTest, AccessesToNewMemory) {
  // This test may corrupt memory if not run under Valgrind.
  if (!RunningOnValgrind())
    return;

  char *foo = new char[10];
  MakeSomeErrors(foo, 10);
  delete [] foo;
  foo[5] = 0;  // Use after delete. This won't break anything under Valgrind.
}

TEST(ToolsSanityTest, AccessesToMallocMemory) {
  // This test may corrupt memory if not run under Valgrind.
  if (!RunningOnValgrind())
    return;
  char *foo = reinterpret_cast<char*>(malloc(10));
  MakeSomeErrors(foo, 10);
  free(foo);
  foo[5] = 0;  // Use after free. This won't break anything under Valgrind.
}

TEST(ToolsSanityTest, ArrayDeletedWithoutBraces) {
  // This test may corrupt memory if not run under Valgrind.
  if (!RunningOnValgrind())
    return;

  // Without the |volatile|, clang optimizes away the next two lines.
  int* volatile foo = new int[10];
  delete foo;
}

TEST(ToolsSanityTest, SingleElementDeletedWithBraces) {
  // This test may corrupt memory if not run under Valgrind.
  if (!RunningOnValgrind())
    return;

  // Without the |volatile|, clang optimizes away the next two lines.
  int* volatile foo = new int;
  delete [] foo;
}


namespace {

// We use caps here just to ensure that the method name doesn't interfere with
// the wildcarded suppressions.
class TOOLS_SANITY_TEST_CONCURRENT_THREAD : public PlatformThread::Delegate {
 public:
  explicit TOOLS_SANITY_TEST_CONCURRENT_THREAD(bool *value) : value_(value) {}
  ~TOOLS_SANITY_TEST_CONCURRENT_THREAD() {}
  void ThreadMain() {
    *value_ = true;

    // Sleep for a few milliseconds so the two threads are more likely to live
    // simultaneously. Otherwise we may miss the report due to mutex
    // lock/unlock's inside thread creation code in pure-happens-before mode...
    PlatformThread::Sleep(100);
  }
 private:
  bool *value_;
};

class ReleaseStoreThread : public PlatformThread::Delegate {
 public:
  explicit ReleaseStoreThread(base::subtle::Atomic32 *value) : value_(value) {}
  ~ReleaseStoreThread() {}
  void ThreadMain() {
    base::subtle::Release_Store(value_, kMagicValue);

    // Sleep for a few milliseconds so the two threads are more likely to live
    // simultaneously. Otherwise we may miss the report due to mutex
    // lock/unlock's inside thread creation code in pure-happens-before mode...
    PlatformThread::Sleep(100);
  }
 private:
  base::subtle::Atomic32 *value_;
};

class AcquireLoadThread : public PlatformThread::Delegate {
 public:
  explicit AcquireLoadThread(base::subtle::Atomic32 *value) : value_(value) {}
  ~AcquireLoadThread() {}
  void ThreadMain() {
    // Wait for the other thread to make Release_Store
    PlatformThread::Sleep(100);
    base::subtle::Acquire_Load(value_);
  }
 private:
  base::subtle::Atomic32 *value_;
};

void RunInParallel(PlatformThread::Delegate *d1, PlatformThread::Delegate *d2) {
  PlatformThreadHandle a;
  PlatformThreadHandle b;
  PlatformThread::Create(0, d1, &a);
  PlatformThread::Create(0, d2, &b);
  PlatformThread::Join(a);
  PlatformThread::Join(b);
}

}  // namespace

// A data race detector should report an error in this test.
TEST(ToolsSanityTest, DataRace) {
  bool shared = false;
  TOOLS_SANITY_TEST_CONCURRENT_THREAD thread1(&shared), thread2(&shared);
  RunInParallel(&thread1, &thread2);
  EXPECT_TRUE(shared);
}

TEST(ToolsSanityTest, AnnotateBenignRace) {
  bool shared = false;
  ANNOTATE_BENIGN_RACE(&shared, "Intentional race - make sure doesn't show up");
  TOOLS_SANITY_TEST_CONCURRENT_THREAD thread1(&shared), thread2(&shared);
  RunInParallel(&thread1, &thread2);
  EXPECT_TRUE(shared);
}

TEST(ToolsSanityTest, AtomicsAreIgnored) {
  base::subtle::Atomic32 shared = 0;
  ReleaseStoreThread thread1(&shared);
  AcquireLoadThread thread2(&shared);
  RunInParallel(&thread1, &thread2);
  EXPECT_EQ(kMagicValue, shared);
}

}  // namespace base
