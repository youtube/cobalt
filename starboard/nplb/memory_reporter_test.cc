// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/memory_reporter.h"
#include "starboard/common/mutex.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// This thread local boolean is used to filter allocations and
// 1) Prevent allocations from other threads.
// 2) Selectively disable allocations so that unintended allocs from
//    gTest (ASSERT_XXX) don't cause the current test to fail.
void InitMemoryTrackingState_ThreadLocal();
bool GetMemoryTrackingEnabled_ThreadLocal();
void SetMemoryTrackingEnabled_ThreadLocal(bool value);

// Scoped object that temporary turns off MemoryTracking. This is needed
// to avoid allocations from gTest being inserted into the memory tracker.
struct NoMemTracking {
  bool prev_val;
  NoMemTracking() {
    prev_val = GetMemoryTrackingEnabled_ThreadLocal();
    SetMemoryTrackingEnabled_ThreadLocal(false);
  }
  ~NoMemTracking() {
    SetMemoryTrackingEnabled_ThreadLocal(prev_val);
  }
};

// EXPECT_XXX and ASSERT_XXX allocate memory, a big no-no when
// for unit testing allocations. These overrides disable memory
// tracking for the duration of the EXPECT and ASSERT operations.
#define EXPECT_EQ_NO_TRACKING(A, B)                \
{ NoMemTracking no_memory_tracking_in_this_scope;  \
  EXPECT_EQ(A, B);                                 \
}

#define EXPECT_NE_NO_TRACKING(A, B)                \
{ NoMemTracking no_memory_tracking_in_this_scope;  \
  EXPECT_NE(A, B);                                 \
}

#define EXPECT_TRUE_NO_TRACKING(A)                 \
{ NoMemTracking no_memory_tracking_in_this_scope;  \
  EXPECT_TRUE(A);                                  \
}

#define EXPECT_FALSE_NO_TRACKING(A)                \
{ NoMemTracking no_memory_tracking_in_this_scope;  \
  EXPECT_FALSE(A);                                 \
}

#define ASSERT_EQ_NO_TRACKING(A, B)                \
{ NoMemTracking no_memory_tracking_in_this_scope;  \
  ASSERT_EQ(A, B);                                 \
}

#define ASSERT_NE_NO_TRACKING(A, B)                \
{ NoMemTracking no_memory_tracking_in_this_scope;  \
  ASSERT_NE(A, B);                                 \
}

#define ASSERT_TRUE_NO_TRACKING(A)                 \
{ NoMemTracking no_memory_tracking_in_this_scope;  \
  ASSERT_TRUE(A);                                  \
}

#define ASSERT_FALSE_NO_TRACKING(A)                \
{ NoMemTracking no_memory_tracking_in_this_scope;  \
  ASSERT_FALSE(A);                                 \
}

// A structure that cannot be allocated because it throws an exception in its
// constructor. This is needed to test the std::nothrow version of delete since
// it is only called when the std::nothrow version of new fails.
struct ThrowConstructor {
  // ThrowConstructor() throw(std::exception) { throw std::exception(); }
  ThrowConstructor() : foo_(1) { throw std::exception(); }
  // Required to prevent the constructor from being inlined and optimized away.
  volatile int foo_;
};

///////////////////////////////////////////////////////////////////////////////
// A memory reporter that is used to watch allocations from the system.
class TestMemReporter {
 public:
  TestMemReporter() { Construct(); }

  SbMemoryReporter* memory_reporter() {
    return &memory_reporter_;
  }

  // Removes this object from listening to allocations.
  void RemoveGlobalHooks() {
    SbMemorySetReporter(NULL);
    // Sleep to allow other threads time to pass through.
    SbThreadSleep(250 * kSbTimeMillisecond);
  }

  // Total number allocations outstanding.
  int number_allocs() const { return number_allocs_; }

  // Total number memory map outstanding.
  int number_map_mem() const { return number_map_mem_; }

  void Clear() {
    starboard::ScopedLock lock(mutex_);
    number_allocs_ = 0;
    number_map_mem_ = 0;
    last_allocation_ = NULL;
    last_deallocation_ = NULL;
    last_mem_map_ = NULL;
    last_mem_unmap_ = NULL;
  }

  const void* last_allocation() const {
    return last_allocation_;
  }
  const void* last_deallocation() const {
    return last_deallocation_;
  }
  const void* last_mem_map() const {
    return last_mem_map_;
  }
  const void* last_mem_unmap() const {
    return last_mem_unmap_;
  }

 private:
  // Boilerplate to delegate static function callbacks to the class instance.
  static void OnMalloc(void* context, const void* memory, size_t size) {
    TestMemReporter* t = static_cast<TestMemReporter*>(context);
    t->ReportAlloc(memory, size);
  }

  static void OnMapMem(void* context, const void* memory, size_t size) {
    TestMemReporter* t = static_cast<TestMemReporter*>(context);
    t->ReportMapMem(memory, size);
  }

  static void OnDealloc(void* context, const void* memory) {
    TestMemReporter* t = static_cast<TestMemReporter*>(context);
    t->ReportDealloc(memory);
  }

  static void SbUnMapMem(void* context, const void* memory, size_t size) {
    TestMemReporter* t = static_cast<TestMemReporter*>(context);
    t->ReportUnMapMemory(memory, size);
  }

  static SbMemoryReporter CreateSbMemoryReporter(TestMemReporter* context) {
    SbMemoryReporter cb = { OnMalloc, OnDealloc,
                            OnMapMem, SbUnMapMem,
                            context };
    return cb;
  }

  void ReportAlloc(const void* memory, size_t size) {
    if (!GetMemoryTrackingEnabled_ThreadLocal()) {
      return;
    }
    starboard::ScopedLock lock(mutex_);
    last_allocation_ = memory;
    number_allocs_++;
  }

  void ReportMapMem(const void* memory, size_t size) {
    if (!GetMemoryTrackingEnabled_ThreadLocal()) {
      return;
    }
    starboard::ScopedLock lock(mutex_);
    last_mem_map_ = memory;
    number_map_mem_++;
  }

  void ReportDealloc(const void* memory) {
    if (!GetMemoryTrackingEnabled_ThreadLocal()) {
      return;
    }
    starboard::ScopedLock lock(mutex_);
    last_deallocation_ = memory;
    number_allocs_--;
  }

  void ReportUnMapMemory(const void* memory, size_t size) {
    if (!GetMemoryTrackingEnabled_ThreadLocal()) {
      return;
    }
    starboard::ScopedLock lock(mutex_);
    last_mem_unmap_ = memory;
    number_map_mem_--;
  }

  void Construct() {
    Clear();
    memory_reporter_ = CreateSbMemoryReporter(this);
  }

  SbMemoryReporter memory_reporter_;
  starboard::Mutex mutex_;
  const void* last_allocation_;
  const void* last_deallocation_;
  const void* last_mem_map_;
  const void* last_mem_unmap_;
  int number_allocs_;
  int number_map_mem_;
};

///////////////////////////////////////////////////////////////////////////////
// Needed by all tests that require a memory tracker to be installed. On the
// first test the test memory tracker is installed and then after the last test
// the memory tracker is removed.
class MemoryReportingTest : public ::testing::Test {
 public:
  TestMemReporter* mem_reporter() { return s_test_alloc_tracker_; }
  bool MemoryReportingEnabled() const {
    return s_memory_reporter_error_enabled_;
  }

 protected:
  // Global setup - runs before the first test in the series.
  static void SetUpTestCase() {
    InitMemoryTrackingState_ThreadLocal();
    // global init test code should run here.
    if (s_test_alloc_tracker_ == NULL) {
      s_test_alloc_tracker_ = new TestMemReporter;
    }
    s_memory_reporter_error_enabled_ =
        SbMemorySetReporter(s_test_alloc_tracker_->memory_reporter());
  }

  // Global Teardown after last test has run it's course.
  static void TearDownTestCase() {
    s_test_alloc_tracker_->RemoveGlobalHooks();
  }

  // Per test setup.
  virtual void SetUp() {
    mem_reporter()->Clear();
    // Allows current thread to capture memory allocations. If this wasn't
    // done then background threads spawned by a framework could notify this
    // class about allocations and fail the test.
    SetMemoryTrackingEnabled_ThreadLocal(true);
  }

  // Per test teardown.
  virtual void TearDown() {
    SetMemoryTrackingEnabled_ThreadLocal(false);
    if ((mem_reporter()->number_allocs() != 0) ||
        (mem_reporter()->number_map_mem() != 0)) {
      ADD_FAILURE_AT(__FILE__, __LINE__) << "Memory Leak detected.";
    }
    mem_reporter()->Clear();
  }
  static TestMemReporter* s_test_alloc_tracker_;
  static bool s_memory_reporter_error_enabled_;
};
TestMemReporter* MemoryReportingTest::s_test_alloc_tracker_ = NULL;
bool MemoryReportingTest::s_memory_reporter_error_enabled_ = false;

///////////////////////////////////////////////////////////////////////////////
// TESTS.
// There are two sets of tests: POSITIVE and NEGATIVE.
//  The positive tests are active when STARBOARD_ALLOWS_MEMORY_TRACKING is
//  defined and test that memory tracking is enabled.
//  NEGATIVE tests ensure that tracking is disabled when
//  STARBOARD_ALLOWS_MEMORY_TRACKING is not defined.
// When adding new tests:
//  POSITIVE tests are named normally.
//  NEGATIVE tets are named with "No" prefixed to the beginning.
//  Example:
//   TEST_F(MemoryReportingTest, CapturesAllocDealloc) <--- POSITIVE test.
//   TEST_F(MemoryReportingTest, NoCapturesAllocDealloc) <- NEGATIVE test.
//  All positive & negative tests are grouped together.
///////////////////////////////////////////////////////////////////////////////

#ifdef STARBOARD_ALLOWS_MEMORY_TRACKING
// These are POSITIVE tests, which test the expectation that when the define
// STARBOARD_ALLOWS_MEMORY_TRACKING is active that the memory tracker will
// receive memory allocations notifications.
//
// Tests the assumption that the SbMemoryAllocate and SbMemoryDeallocate
// will report memory allocations.
TEST_F(MemoryReportingTest, CapturesAllocDealloc) {
  if (!MemoryReportingEnabled()) {
    SbLog(kSbLogPriorityInfo, "Memory reporting is disabled.\n");
    return;
  }
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_allocs());
  void* memory = SbMemoryAllocate(4);
  EXPECT_EQ_NO_TRACKING(1, mem_reporter()->number_allocs());

  EXPECT_EQ_NO_TRACKING(memory, mem_reporter()->last_allocation());

  SbMemoryDeallocate(memory);
  EXPECT_EQ_NO_TRACKING(mem_reporter()->number_allocs(), 0);

  // Should equal the last allocation.
  EXPECT_EQ_NO_TRACKING(memory, mem_reporter()->last_deallocation());
}

// Tests the assumption that SbMemoryReallocate() will report a
// deallocation and an allocation to the memory tracker.
TEST_F(MemoryReportingTest, CapturesRealloc) {
  if (!MemoryReportingEnabled()) {
    SbLog(kSbLogPriorityInfo, "Memory reporting is disabled.\n");
    return;
  }
  void* prev_memory = SbMemoryAllocate(4);
  void* new_memory = SbMemoryReallocate(prev_memory, 8);

  EXPECT_EQ_NO_TRACKING(new_memory, mem_reporter()->last_allocation());
  EXPECT_EQ_NO_TRACKING(prev_memory, mem_reporter()->last_deallocation());

  EXPECT_EQ_NO_TRACKING(1, mem_reporter()->number_allocs());

  SbMemoryDeallocate(new_memory);
  EXPECT_EQ_NO_TRACKING(mem_reporter()->number_allocs(), 0);
}

#if SB_API_VERSION >= 12 || SB_HAS(MMAP)
// Tests the assumption that the SbMemoryMap and SbMemoryUnmap
// will report memory allocations.
TEST_F(MemoryReportingTest, CapturesMemMapUnmap) {
  if (!MemoryReportingEnabled()) {
    SbLog(kSbLogPriorityInfo, "Memory reporting is disabled.\n");
    return;
  }
  const int64_t kMemSize = 4096;
  const int kFlags = kSbMemoryMapProtectReadWrite;
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_map_mem());
  void* mem_chunk = SbMemoryMap(kMemSize, kFlags, "TestMemMap");
  EXPECT_EQ_NO_TRACKING(1, mem_reporter()->number_map_mem());

  // Now unmap the memory and confirm that this memory was reported as free.
  EXPECT_EQ_NO_TRACKING(mem_chunk, mem_reporter()->last_mem_map());
  SbMemoryUnmap(mem_chunk, kMemSize);
  EXPECT_EQ_NO_TRACKING(mem_chunk, mem_reporter()->last_mem_unmap());
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_map_mem());
  // On some platforms, bookkeeping for memory mapping can cost allocations.
  // Call Clear() explicitly before TearDown() checks number_allocs_;
  mem_reporter()->Clear();
}
#endif  // SB_API_VERSION >= 12 || SB_HAS(MMAP)

// Tests the assumption that the operator new/delete will report
// memory allocations.
TEST_F(MemoryReportingTest, CapturesOperatorNewDelete) {
  if (!MemoryReportingEnabled()) {
    SbLog(kSbLogPriorityInfo, "Memory reporting is disabled.\n");
    return;
  }
  EXPECT_TRUE_NO_TRACKING(mem_reporter()->number_allocs() == 0);
  int* my_int = new int();
  EXPECT_TRUE_NO_TRACKING(mem_reporter()->number_allocs() == 1);

  bool is_last_allocation =
      my_int == mem_reporter()->last_allocation();

  EXPECT_TRUE_NO_TRACKING(is_last_allocation);

  delete my_int;
  EXPECT_TRUE_NO_TRACKING(mem_reporter()->number_allocs() == 0);

  // Expect last deallocation to be the expected pointer.
  EXPECT_EQ_NO_TRACKING(my_int, mem_reporter()->last_deallocation());
}

// Tests the assumption that the nothrow version of operator new will report
// memory allocations.
TEST_F(MemoryReportingTest, CapturesOperatorNewNothrow) {
  if (!MemoryReportingEnabled()) {
    SbLog(kSbLogPriorityInfo, "Memory reporting is disabled.\n");
    return;
  }
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_allocs());
  int* my_int = new (std::nothrow) int();
  EXPECT_EQ_NO_TRACKING(1, mem_reporter()->number_allocs());

  bool is_last_allocation =
      my_int == mem_reporter()->last_allocation();

  EXPECT_TRUE_NO_TRACKING(is_last_allocation);

  delete my_int;
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_allocs());

  // Expect last deallocation to be the expected pointer.
  EXPECT_EQ_NO_TRACKING(my_int, mem_reporter()->last_deallocation());
}

// Tests the assumption that the nothrow version of operator delete will report
// memory deallocations.
TEST_F(MemoryReportingTest, CapturesOperatorDeleteNothrow) {
  if (!MemoryReportingEnabled()) {
    SbLog(kSbLogPriorityInfo, "Memory reporting is disabled.\n");
    return;
  }
  const void* init_alloc = mem_reporter()->last_allocation();

  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_allocs());
  void* my_obj = nullptr;
  bool caught_exception = false;
  try {
    my_obj = new (std::nothrow) ThrowConstructor();
  } catch (std::exception e) {
    caught_exception = true;
  }
  EXPECT_TRUE(caught_exception);
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_allocs());

  // Expect that an allocation occurred, even though we never got a pointer.
  EXPECT_EQ_NO_TRACKING(nullptr, my_obj);
  EXPECT_NE_NO_TRACKING(nullptr, mem_reporter()->last_allocation());
  EXPECT_NE_NO_TRACKING(init_alloc, mem_reporter()->last_allocation());

  // Expect last deallocation to be the allocation we never got.
  EXPECT_EQ_NO_TRACKING(mem_reporter()->last_allocation(),
                        mem_reporter()->last_deallocation());
}

#else  // !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
// These are NEGATIVE tests, which test the expectation that when the
// STARBOARD_ALLOWS_MEMORY_TRACKING is undefined that the memory tracker does
// not receive memory allocations notifications.

TEST_F(MemoryReportingTest, NoCapturesAllocDealloc) {
  EXPECT_FALSE_NO_TRACKING(MemoryReportingEnabled());

  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_allocs());
  void* memory = SbMemoryAllocate(4);
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_allocs());
  EXPECT_EQ_NO_TRACKING(NULL, mem_reporter()->last_allocation());

  SbMemoryDeallocate(memory);
  EXPECT_EQ_NO_TRACKING(mem_reporter()->number_allocs(), 0);
  EXPECT_EQ_NO_TRACKING(NULL, mem_reporter()->last_deallocation());
}

TEST_F(MemoryReportingTest, NoCapturesRealloc) {
  EXPECT_FALSE_NO_TRACKING(MemoryReportingEnabled());
  void* prev_memory = SbMemoryAllocate(4);
  void* new_memory = SbMemoryReallocate(prev_memory, 8);

  EXPECT_EQ_NO_TRACKING(NULL, mem_reporter()->last_allocation());
  EXPECT_EQ_NO_TRACKING(NULL, mem_reporter()->last_deallocation());
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_allocs());

  SbMemoryDeallocate(new_memory);
  EXPECT_EQ_NO_TRACKING(mem_reporter()->number_allocs(), 0);
}

TEST_F(MemoryReportingTest, NoCapturesMemMapUnmap) {
  EXPECT_FALSE_NO_TRACKING(MemoryReportingEnabled());
  const int64_t kMemSize = 4096;
  const int kFlags = 0;
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_allocs());
  void* mem_chunk = SbMemoryMap(kMemSize, kFlags, "TestMemMap");
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_allocs());

  // Now unmap the memory and confirm that this memory was reported as free.
  EXPECT_EQ_NO_TRACKING(NULL, mem_reporter()->last_mem_map());
  SbMemoryUnmap(mem_chunk, kMemSize);
  EXPECT_EQ_NO_TRACKING(NULL, mem_reporter()->last_mem_unmap());
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_allocs());
}

TEST_F(MemoryReportingTest, NoCapturesOperatorNewDelete) {
  EXPECT_FALSE_NO_TRACKING(MemoryReportingEnabled());
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_map_mem());
  int* my_int = new int();
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_map_mem());
  EXPECT_EQ_NO_TRACKING(NULL, mem_reporter()->last_allocation());

  delete my_int;
  EXPECT_EQ_NO_TRACKING(0, mem_reporter()->number_map_mem());
  EXPECT_EQ_NO_TRACKING(NULL, mem_reporter()->last_deallocation());
}

#endif  // !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)

/////////////////////////////// Implementation ////////////////////////////////

// Simple ThreadLocalBool class which allows a default value to be
// true or false.
class ThreadLocalBool {
 public:
  ThreadLocalBool() {
    // NULL is the destructor.
    slot_ = SbThreadCreateLocalKey(NULL);
  }

  ~ThreadLocalBool() {
    SbThreadDestroyLocalKey(slot_);
  }

  void SetEnabled(bool value) {
    SetEnabledThreadLocal(value);
  }

  bool Enabled() const {
    return GetEnabledThreadLocal();
  }

 private:
  void SetEnabledThreadLocal(bool value) {
    void* bool_as_pointer;
    if (value) {
      bool_as_pointer = reinterpret_cast<void*>(0x1);
    } else {
      bool_as_pointer = NULL;
    }
    SbThreadSetLocalValue(slot_, bool_as_pointer);
  }

  bool GetEnabledThreadLocal() const {
    void* ptr = SbThreadGetLocalValue(slot_);
    return ptr != NULL;
  }

  mutable SbThreadLocalKey slot_;
  bool default_value_;
};

void InitMemoryTrackingState_ThreadLocal() {
  GetMemoryTrackingEnabled_ThreadLocal();
}

static ThreadLocalBool* GetMemoryTrackingState() {
  static ThreadLocalBool* thread_local_bool = new ThreadLocalBool();
  return thread_local_bool;
}

bool GetMemoryTrackingEnabled_ThreadLocal() {
  return GetMemoryTrackingState()->Enabled();
}

void SetMemoryTrackingEnabled_ThreadLocal(bool value) {
  GetMemoryTrackingState()->SetEnabled(value);
}

// No use for these macros anymore.
#undef EXPECT_EQ_NO_TRACKING
#undef EXPECT_TRUE_NO_TRACKING
#undef EXPECT_FALSE_NO_TRACKING
#undef ASSERT_EQ_NO_TRACKING
#undef ASSERT_TRUE_NO_TRACKING
#undef ASSERT_FALSE_NO_TRACKING

}  // namespace
}  // namespace nplb
}  // namespace starboard
