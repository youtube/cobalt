/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nb/analytics/memory_tracker_impl.h"
#include "nb/memory_scope.h"
#include "nb/scoped_ptr.h"
#include "nb/test_thread.h"
#include "starboard/configuration.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

#define STRESS_TEST_DURATION_SECONDS 1
#define NUM_STRESS_TEST_THREADS 3

// The following is necessary to prevent operator new from being optimized
// out using some compilers. This is required because we rely on operator new
// to report memory usage. Overly aggressive optimizing compilers will
// eliminate the call to operator new, even though it causes a lost of side
// effects. This will therefore break the memory reporting mechanism. This is
// a bug in the compiler.
//
// The solution here is to use macro-replacement to substitute calls to global
// new to instead be delegated to our custom new, which prevents elimination
// by using a temporay volatile.
namespace {
struct CustomObject {
  static CustomObject Make() {
    CustomObject o;
    return o;
  }
};
}

void* operator new(std::size_t size, CustomObject ignored) {
  SB_UNREFERENCED_PARAMETER(ignored);
  // Volatile prevents optimization and elimination of operator new by the
  // optimizing compiler.
  volatile void* ptr = ::operator new(size);
  return const_cast<void*>(ptr);
}

#define NEW_NO_OPTIMIZER_ELIMINATION new (CustomObject::Make())
#define new NEW_NO_OPTIMIZER_ELIMINATION

namespace nb {
namespace analytics {
namespace {

class NoReportAllocator {
 public:
  NoReportAllocator() {}
  NoReportAllocator(const NoReportAllocator&) {}
  static void* Allocate(size_t n) {
    return SbMemoryAllocateNoReport(n);
  }
  // Second argument can be used for accounting, but is otherwise optional.
  static void Deallocate(void* ptr, size_t /* not used*/) {
    SbMemoryDeallocateNoReport(ptr);
  }
};

// Some platforms will allocate memory for empty vectors. Therefore
// use MyVector to prevent empty vectors from reporting memory.
template <typename T>
using MyVector = std::vector<T, StdAllocator<T, NoReportAllocator>>;

MemoryTrackerImpl* s_memory_tracker_ = NULL;

struct NoMemTracking {
  bool prev_val;
  NoMemTracking() : prev_val(false) {
    if (s_memory_tracker_) {
      prev_val = s_memory_tracker_->IsMemoryTrackingEnabled();
      s_memory_tracker_->SetMemoryTrackingEnabled(false);
    }
  }
  ~NoMemTracking() {
    if (s_memory_tracker_) {
      s_memory_tracker_->SetMemoryTrackingEnabled(prev_val);
    }
  }
};

// EXPECT_XXX and ASSERT_XXX allocate memory, a big no-no when
// for unit testing allocations. These overrides disable memory
// tracking for the duration of the EXPECT and ASSERT operations.
#define EXPECT_EQ_NO_TRACKING(A, B)                 \
  {                                                 \
    NoMemTracking no_memory_tracking_in_this_scope; \
    EXPECT_EQ(A, B);                                \
  }

#define EXPECT_TRUE_NO_TRACKING(A)                  \
  {                                                 \
    NoMemTracking no_memory_tracking_in_this_scope; \
    EXPECT_TRUE(A);                                 \
  }

#define EXPECT_FALSE_NO_TRACKING(A)                 \
  {                                                 \
    NoMemTracking no_memory_tracking_in_this_scope; \
    EXPECT_FALSE(A);                                \
  }

#define ASSERT_EQ_NO_TRACKING(A, B)                 \
  {                                                 \
    NoMemTracking no_memory_tracking_in_this_scope; \
    ASSERT_EQ(A, B);                                \
  }

#define ASSERT_TRUE_NO_TRACKING(A)                  \
  {                                                 \
    NoMemTracking no_memory_tracking_in_this_scope; \
    ASSERT_TRUE(A);                                 \
  }

// !! converts int -> bool.
bool FlipCoin() {
  return !!(SbSystemGetRandomUInt64() & 0x1);
}

///////////////////////////////////////////////////////////////////////////////
// Stress testing the Allocation Tracker.
class MemoryScopeThread : public nb::TestThread {
 public:
  typedef nb::TestThread Super;

  explicit MemoryScopeThread(MemoryTrackerImpl* memory_tracker)
      : memory_tracker_(memory_tracker) {
    static int s_counter = 0;

    std::stringstream ss;
    ss << "MemoryScopeThread_" << s_counter++;
    unique_name_ = ss.str();
  }
  virtual ~MemoryScopeThread() {}

  // Overridden so that the thread can exit gracefully.
  virtual void Join() {
    finished_ = true;
    Super::Join();
  }
  virtual void Run() {
    while (!finished_) {
      TRACK_MEMORY_SCOPE_DYNAMIC(unique_name_.c_str());
      AllocationGroup* group = memory_tracker_->PeekAllocationGroup();

      const int cmp_result = group->name().compare(unique_name_);
      if (cmp_result != 0) {
        GTEST_FAIL() << "unique name mismatch";
        return;
      }
    }
  }

 private:
  MemoryTrackerImpl* memory_tracker_;
  bool finished_;
  std::string unique_name_;
  int do_delete_counter_;
  int do_malloc_counter_;
};

///////////////////////////////////////////////////////////////////////////////
// Stress testing the Allocation Tracker.
class AllocationStressThread : public nb::TestThread {
 public:
  explicit AllocationStressThread(MemoryTrackerImpl* memory_tracker);
  virtual ~AllocationStressThread();

  // Overridden so that the thread can exit gracefully.
  virtual void Join();
  virtual void Run();

 private:
  typedef std::map<const void*, AllocationRecord> AllocMap;

  void CheckPointers();
  bool RemoveRandomAllocation(std::pair<const void*, AllocationRecord>* output);
  bool DoDelete();
  void DoMalloc();

  MemoryTrackerImpl* memory_tracker_;
  bool finished_;
  std::map<const void*, AllocationRecord> allocated_pts_;
  std::string unique_name_;
  int do_delete_counter_;
  int do_malloc_counter_;
};

class AddAllocationStressThread : public nb::TestThread {
 public:
  typedef std::map<const void*, AllocationRecord> AllocMap;

  AddAllocationStressThread(MemoryTracker* memory_tracker,
                            int num_elements_add,
                            AllocMap* destination_map,
                            starboard::Mutex* destination_map_mutex)
      : memory_tracker_(memory_tracker),
        num_elements_to_add_(num_elements_add),
        destination_map_(destination_map),
        destination_map_mutex_(destination_map_mutex) {}

  virtual void Run() {
    for (int i = 0; i < num_elements_to_add_; ++i) {
      const int alloc_size = std::rand() % 100 + 8;
      void* ptr = SbMemoryAllocate(alloc_size);

      AllocationRecord record;
      if (memory_tracker_->GetMemoryTracking(ptr, &record)) {
        NoMemTracking no_mem_tracking;  // simplifies test.

        starboard::ScopedLock lock(*destination_map_mutex_);
        destination_map_->insert(std::make_pair(ptr, record));
      } else {
        ADD_FAILURE_AT(__FILE__, __LINE__) << "Could not add pointer.";
      }
      if (FlipCoin()) {
        SbThreadYield();  // Give other threads a chance to run.
      }
    }
  }

 private:
  MemoryTracker* memory_tracker_;
  AllocMap* destination_map_;
  starboard::Mutex* destination_map_mutex_;
  int num_elements_to_add_;
};

///////////////////////////////////////////////////////////////////////////////
// Framework which initializes the MemoryTracker once and installs it
// for the first test and the removes the MemoryTracker after the
// the last test finishes.
class MemoryTrackerImplTest : public ::testing::Test {
 public:
  typedef MemoryTrackerImpl::AllocationMapType AllocationMapType;
  MemoryTrackerImplTest() {}

  MemoryTrackerImpl* memory_tracker() { return s_memory_tracker_; }

  bool GetAllocRecord(void* alloc_memory, AllocationRecord* output) {
    return memory_tracker()->GetMemoryTracking(alloc_memory, output);
  }

  AllocationMapType* pointer_map() { return memory_tracker()->pointer_map(); }

  size_t NumberOfAllocations() {
    AllocationMapType* map = pointer_map();
    return map->Size();
  }

  int64_t TotalAllocationBytes() {
    return memory_tracker()->GetTotalAllocationBytes();
  }

  bool MemoryTrackerEnabled() const { return s_memory_tracker_enabled_; }

 protected:
  static void SetUpTestCase() {
    if (!s_memory_tracker_) {
      s_memory_tracker_ = new MemoryTrackerImpl;
    }
    // There are obligatory background threads for nb_test suite. This filter
    // makes sure that they don't intercept this test.
    s_memory_tracker_->SetThreadFilter(SbThreadGetId());
    s_memory_tracker_enabled_ =
        s_memory_tracker_->InstallGlobalTrackingHooks();
  }
  static void TearDownTestCase() {
    s_memory_tracker_->RemoveGlobalTrackingHooks();
    // Give time for threads to sync. We don't use locks on the reporter
    // for performance reasons.
    SbThreadSleep(250 * kSbTimeMillisecond);
  }

  virtual void SetUp() {
    memory_tracker()->Clear();
  }

  virtual void TearDown() {
    memory_tracker()->Clear();
  }
  static bool s_memory_tracker_enabled_;
};
bool MemoryTrackerImplTest::s_memory_tracker_enabled_ = false;

///////////////////////////////////////////////////////////////////////////////
// MemoryTrackerImplTest
TEST_F(MemoryTrackerImplTest, NoMemTracking) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  ASSERT_EQ_NO_TRACKING(0, NumberOfAllocations());
  scoped_ptr<int> dummy(new int());
  EXPECT_EQ_NO_TRACKING(1, NumberOfAllocations());
  {
    // Now that memory allocation is disabled, no more allocations should
    // be recorded.
    NoMemTracking no_memory_tracking_in_this_scope;
    int* dummy2 = new int();
    EXPECT_EQ_NO_TRACKING(1, NumberOfAllocations());
    delete dummy2;
    EXPECT_EQ_NO_TRACKING(1, NumberOfAllocations());
  }
  scoped_ptr<int> dummy2(new int());
  EXPECT_EQ_NO_TRACKING(2, NumberOfAllocations());
  dummy.reset(NULL);
  EXPECT_EQ_NO_TRACKING(1, NumberOfAllocations());
  dummy2.reset(NULL);
  EXPECT_EQ_NO_TRACKING(0, NumberOfAllocations());
}

TEST_F(MemoryTrackerImplTest, RemovePointerOnNoMemoryTracking) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }

  int* int_ptr = new int();
  {
    NoMemTracking no_memory_tracking_in_this_scope;
    delete int_ptr;
  }
  EXPECT_FALSE_NO_TRACKING(pointer_map()->Get(int_ptr, NULL));
}

TEST_F(MemoryTrackerImplTest, NewDeleteOverridenTest) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  EXPECT_EQ_NO_TRACKING(0, NumberOfAllocations());
  int* int_a = new int(0);
  EXPECT_EQ_NO_TRACKING(1, NumberOfAllocations());
  delete int_a;
  EXPECT_EQ_NO_TRACKING(0, NumberOfAllocations());
}

TEST_F(MemoryTrackerImplTest, TotalAllocationBytes) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  int32_t* int_a = new int32_t(0);
  EXPECT_EQ_NO_TRACKING(1, NumberOfAllocations());
  EXPECT_EQ_NO_TRACKING(4, TotalAllocationBytes());
  delete int_a;
  EXPECT_EQ_NO_TRACKING(0, NumberOfAllocations());
}

// Tests the expectation that a lot of allocations can be executed and that
// internal data structures won't overflow.
TEST_F(MemoryTrackerImplTest, NoStackOverflow) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  static const int kNumAllocations = 1000;
  MyVector<int*> allocations;

  // Also it turns out that this test is great for catching
  // background threads pushing allocations through the allocator.
  // This is supposed to be filtered, but if it's not then this test will
  // fail.
  // SbThreadYield() is used to give other threads a chance to enter into our
  // allocator and catch a test failure.
  SbThreadSleep(1);
  size_t previously_existing_allocs = NumberOfAllocations();
  ASSERT_EQ_NO_TRACKING(0, previously_existing_allocs);

  for (int i = 0; i < kNumAllocations; ++i) {
    SbThreadYield();
    EXPECT_EQ_NO_TRACKING(i, NumberOfAllocations());
    int* val = new int(0);
    NoMemTracking no_tracking_in_scope;
    allocations.push_back(val);
  }

  EXPECT_EQ_NO_TRACKING(kNumAllocations, NumberOfAllocations());

  for (int i = 0; i < kNumAllocations; ++i) {
    SbThreadYield();
    EXPECT_EQ_NO_TRACKING(kNumAllocations - i, NumberOfAllocations());
    delete allocations[i];
  }

  EXPECT_EQ_NO_TRACKING(0, NumberOfAllocations());
}

// Tests the expectation that the macros will push/pop the memory scope.
TEST_F(MemoryTrackerImplTest, MacrosPushPop) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  scoped_ptr<int> dummy;
  {
    TRACK_MEMORY_SCOPE("TestAllocations");
    dummy.reset(new int());
  }

  scoped_ptr<int> dummy2(new int());

  AllocationRecord alloc_rec;
  pointer_map()->Get(dummy.get(), &alloc_rec);

  ASSERT_TRUE_NO_TRACKING(alloc_rec.allocation_group);
  EXPECT_EQ_NO_TRACKING(std::string("TestAllocations"),
                        alloc_rec.allocation_group->name());

  pointer_map()->Get(dummy2.get(), &alloc_rec);

  ASSERT_TRUE_NO_TRACKING(alloc_rec.allocation_group);
  EXPECT_EQ_NO_TRACKING(std::string("Unaccounted"),
                        alloc_rec.allocation_group->name());
}

// Tests the expectation that if the cached flag on the NbMemoryScopeInfo is
// set to false that the caching of the handle is not performed.
TEST_F(MemoryTrackerImplTest, RespectsNonCachedHandle) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  const bool kCaching = false;
  NbMemoryScopeInfo memory_scope = {
      NULL, "MyName", __FILE__,
      __LINE__, __FUNCTION__, false};  // false to disallow caching.

  // Pushing the memory scope should trigger the caching operation to be
  // attempted. However, because caching was explicitly disabled this handle
  // should retain the value of 0.
  NbPushMemoryScope(&memory_scope);
  EXPECT_TRUE_NO_TRACKING(memory_scope.cached_handle_ == NULL);

  // ... and still assert that the group was created with the expected name.
  AllocationGroup* group = memory_tracker()->GetAllocationGroup("MyName");
  // Equality check.
  EXPECT_EQ_NO_TRACKING(0, group->name().compare("MyName"));
  NbPopMemoryScope();
}

// Tests the expectation that if the cached flag on the NbMemoryScopeInfo is
// set to true that the caching will be applied for the cached_handle of the
// memory scope.
TEST_F(MemoryTrackerImplTest, PushAllocGroupCachedHandle) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  NbMemoryScopeInfo memory_scope = {
      NULL,      // Cached handle.
      "MyName",  // Memory scope name.
      __FILE__, __LINE__, __FUNCTION__,
      true  // Allows caching.
  };

  NbPushMemoryScope(&memory_scope);
  EXPECT_TRUE_NO_TRACKING(memory_scope.cached_handle_ != NULL);
  AllocationGroup* group = memory_tracker()->GetAllocationGroup("MyName");

  EXPECT_EQ_NO_TRACKING(memory_scope.cached_handle_,
                        static_cast<void*>(group));
  NbPopMemoryScope();
}

// Tests the expectation that the macro TRACK_MEMORY_SCOPE will capture the
// allocation in the MemoryTrackerImpl.
TEST_F(MemoryTrackerImplTest, MacrosGroupAccounting) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  MemoryTrackerImpl* track_alloc = memory_tracker();  // Debugging.
  track_alloc->Clear();

  memory_tracker()->Clear();
  const AllocationGroup* group_a =
      memory_tracker()->GetAllocationGroup("MemoryTrackerTest-ScopeA");

  const AllocationGroup* group_b =
      memory_tracker()->GetAllocationGroup("MemoryTrackerTest-ScopeB");

  ASSERT_TRUE_NO_TRACKING(group_a);
  ASSERT_TRUE_NO_TRACKING(group_b);

  int32_t num_allocations = -1;
  int64_t allocation_bytes = -1;

  // Expect that both groups have no allocations in them.
  group_a->GetAggregateStats(&num_allocations, &allocation_bytes);
  EXPECT_EQ_NO_TRACKING(0, num_allocations);
  EXPECT_EQ_NO_TRACKING(0, allocation_bytes);

  group_b->GetAggregateStats(&num_allocations, &allocation_bytes);
  EXPECT_EQ_NO_TRACKING(0, num_allocations);
  EXPECT_EQ_NO_TRACKING(0, allocation_bytes);

  scoped_ptr<int> alloc_a, alloc_b, alloc_b2;
  {
    TRACK_MEMORY_SCOPE("MemoryTrackerTest-ScopeA");
    alloc_a.reset(new int());
    {
      TRACK_MEMORY_SCOPE("MemoryTrackerTest-ScopeB");
      alloc_b.reset(new int());
      alloc_b2.reset(new int());

      group_a->GetAggregateStats(&num_allocations, &allocation_bytes);
      EXPECT_EQ_NO_TRACKING(1, num_allocations);
      EXPECT_EQ_NO_TRACKING(4, allocation_bytes);
      alloc_a.reset(NULL);
      group_a->GetAggregateStats(&num_allocations, &allocation_bytes);
      EXPECT_EQ_NO_TRACKING(0, num_allocations);
      EXPECT_EQ_NO_TRACKING(0, allocation_bytes);

      allocation_bytes = num_allocations = -1;
      group_b->GetAggregateStats(&num_allocations, &allocation_bytes);
      EXPECT_EQ_NO_TRACKING(2, num_allocations);
      EXPECT_EQ_NO_TRACKING(8, allocation_bytes);

      alloc_b2.reset(NULL);
      group_b->GetAggregateStats(&num_allocations, &allocation_bytes);
      EXPECT_EQ_NO_TRACKING(1, num_allocations);
      EXPECT_EQ_NO_TRACKING(4, allocation_bytes);

      alloc_b.reset(NULL);
      group_b->GetAggregateStats(&num_allocations, &allocation_bytes);
      EXPECT_EQ_NO_TRACKING(0, num_allocations);
      EXPECT_EQ_NO_TRACKING(0, allocation_bytes);
    }
  }
}

// Tests the expectation that the MemoryTrackerDebugCallback works as expected
// to notify of incoming allocations.
TEST_F(MemoryTrackerImplTest, MemoryTrackerDebugCallback) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }

  // Impl of the callback. Copies the allocation information so that we can
  // ensure it produces expected values.
  class MemoryTrackerDebugCallbackTest : public MemoryTrackerDebugCallback {
   public:
    MemoryTrackerDebugCallbackTest() { Reset(); }
    virtual void OnMemoryAllocation(
        const void* memory_block,
        const AllocationRecord& record,
        const CallStack& callstack) SB_OVERRIDE {
      last_memory_block_allocated_ = memory_block;
      last_allocation_record_allocated_ = record;
    }
    virtual void OnMemoryDeallocation(
        const void* memory_block,
        const AllocationRecord& record,
        const CallStack& callstack) SB_OVERRIDE {
      last_memory_block_deallocated_ = memory_block;
      last_allocation_record_deallocated_ = record;
    }
    void Reset() {
      last_memory_block_allocated_ = NULL;
      last_memory_block_deallocated_ = NULL;
      last_allocation_record_allocated_ = AllocationRecord::Empty();
      last_allocation_record_deallocated_ = AllocationRecord::Empty();
    }
    const void* last_memory_block_allocated_;
    const void* last_memory_block_deallocated_;
    AllocationRecord last_allocation_record_allocated_;
    AllocationRecord last_allocation_record_deallocated_;
  };

  // Needs to be static due to concurrent and lockless nature of object.
  static MemoryTrackerDebugCallbackTest s_debug_callback;
  s_debug_callback.Reset();

  memory_tracker()->SetMemoryTrackerDebugCallback(&s_debug_callback);
  void* memory_block = SbMemoryAllocate(8);
  EXPECT_EQ_NO_TRACKING(
      memory_block,
      s_debug_callback.last_memory_block_allocated_);
  EXPECT_EQ_NO_TRACKING(
      8,
      s_debug_callback.last_allocation_record_allocated_.size);
  // ... and no memory should have been deallocated.
  EXPECT_TRUE_NO_TRACKING(s_debug_callback.last_memory_block_deallocated_
                          == NULL);

  // After this call we check that the callback for deallocation was used.
  SbMemoryDeallocate(memory_block);
  EXPECT_EQ_NO_TRACKING(
      memory_block,
      s_debug_callback.last_memory_block_deallocated_);

  EXPECT_EQ_NO_TRACKING(
      s_debug_callback.last_allocation_record_deallocated_.size,
      8);
}

// Tests the expectation that the visitor can access the allocations.
TEST_F(MemoryTrackerImplTest, VisitorAccess) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  class SimpleVisitor : public AllocationVisitor {
   public:
    SimpleVisitor() : num_memory_allocs_(0) {}
    virtual bool Visit(const void* memory,
                       const AllocationRecord& alloc_record) {
      num_memory_allocs_++;
      return true;  // Keep traversing.
    }

    size_t num_memory_allocs_;
  };

  SimpleVisitor visitor;
  scoped_ptr<int> int_ptr(new int);

  // Should see the int_ptr allocation.
  memory_tracker()->Accept(&visitor);
  EXPECT_EQ_NO_TRACKING(1, visitor.num_memory_allocs_);
  visitor.num_memory_allocs_ = 0;

  int_ptr.reset(NULL);
  // Now no allocations should be available.
  memory_tracker()->Accept(&visitor);
  EXPECT_EQ_NO_TRACKING(0, visitor.num_memory_allocs_);
}

// A stress test that rapidly adds allocations, but saves all deletions
// for the main thread. This test will catch concurrency errors related
// to reporting new allocations.
TEST_F(MemoryTrackerImplTest, MultiThreadedStressAddTest) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  // Disable allocation filtering.
  memory_tracker()->SetThreadFilter(kSbThreadInvalidId);

  MyVector<nb::TestThread*> threads;

  const int kNumObjectsToAdd = 10000 / NUM_STRESS_TEST_THREADS;
  AddAllocationStressThread::AllocMap map;
  starboard::Mutex map_mutex;

  for (int i = 0; i < NUM_STRESS_TEST_THREADS; ++i) {
    nb::TestThread* thread = new AddAllocationStressThread(
        memory_tracker(), kNumObjectsToAdd, &map, &map_mutex);

    threads.push_back(thread);
  }

  for (int i = 0; i < NUM_STRESS_TEST_THREADS; ++i) {
    threads[i]->Start();
  }

  for (int i = 0; i < NUM_STRESS_TEST_THREADS; ++i) {
    threads[i]->Join();
  }
  for (int i = 0; i < NUM_STRESS_TEST_THREADS; ++i) {
    delete threads[i];
  }

  while (!map.empty()) {
    const void* ptr = map.begin()->first;
    map.erase(map.begin());

    if (!memory_tracker()->GetMemoryTracking(ptr, NULL)) {
      ADD_FAILURE_AT(__FILE__, __LINE__) << "No tracking?!";
    }

    SbMemoryDeallocate(const_cast<void*>(ptr));
    if (memory_tracker()->GetMemoryTracking(ptr, NULL)) {
      ADD_FAILURE_AT(__FILE__, __LINE__) << "Tracking?!";
    }
  }
}

// Tests the expectation that memory scopes are multi-threaded safe.
TEST_F(MemoryTrackerImplTest, MultiThreadedMemoryScope) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  memory_tracker()->SetThreadFilter(kSbThreadInvalidId);
  TRACK_MEMORY_SCOPE("MultiThreadedStressUseTest");

  MyVector<MemoryScopeThread*> threads;

  for (int i = 0; i < NUM_STRESS_TEST_THREADS; ++i) {
    threads.push_back(new MemoryScopeThread(memory_tracker()));
  }

  for (int i = 0; i < threads.size(); ++i) {
    threads[i]->Start();
  }

  SbThreadSleep(STRESS_TEST_DURATION_SECONDS * 1000 * 1000);

  for (int i = 0; i < threads.size(); ++i) {
    threads[i]->Join();
  }

  for (int i = 0; i < threads.size(); ++i) {
    delete threads[i];
  }

  threads.clear();
}

// Tests the expectation that new/delete can be done by different threads.
TEST_F(MemoryTrackerImplTest, MultiThreadedStressUseTest) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }
  // Disable allocation filtering.
  memory_tracker()->SetThreadFilter(kSbThreadInvalidId);
  TRACK_MEMORY_SCOPE("MultiThreadedStressUseTest");

  MyVector<AllocationStressThread*> threads;

  for (int i = 0; i < NUM_STRESS_TEST_THREADS; ++i) {
    threads.push_back(new AllocationStressThread(memory_tracker()));
  }

  for (int i = 0; i < threads.size(); ++i) {
    threads[i]->Start();
  }

  SbThreadSleep(STRESS_TEST_DURATION_SECONDS * 1000 * 1000);

  for (int i = 0; i < threads.size(); ++i) {
    threads[i]->Join();
  }

  for (int i = 0; i < threads.size(); ++i) {
    delete threads[i];
  }

  threads.clear();
}

//////////////////////////// Implementation ///////////////////////////////////
/// Impl of AllocationStressThread
AllocationStressThread::AllocationStressThread(MemoryTrackerImpl* tracker)
    : memory_tracker_(tracker), finished_(false) {
  static int counter = 0;
  std::stringstream ss;
  ss << "AllocStressThread-" << counter++;
  unique_name_ = ss.str();
}

AllocationStressThread::~AllocationStressThread() {
  if (!allocated_pts_.empty()) {
    ADD_FAILURE_AT(__FILE__, __LINE__) << "allocated pointers still exist";
  }
}

void AllocationStressThread::Join() {
  finished_ = true;
  nb::TestThread::Join();
}

void AllocationStressThread::CheckPointers() {
  typedef AllocMap::iterator Iter;

  for (Iter it = allocated_pts_.begin(); it != allocated_pts_.end(); ++it) {
    const void* ptr = it->first;
    const bool found = memory_tracker_->GetMemoryTracking(ptr, NULL);
    if (!found) {
      NoMemTracking no_tracking_in_scope;
      ADD_FAILURE_AT(__FILE__, __LINE__) << "Not found";
    }
  }
}

void AllocationStressThread::Run() {
  while (!finished_) {
    const bool do_delete = FlipCoin();
    if (FlipCoin()) {
      DoDelete();
    } else {
      DoMalloc();
    }
    CheckPointers();

    // Randomly give other threads the opportunity run.
    if (FlipCoin()) {
      SbThreadYield();
    }
  }

  // Clear out all memory.
  while (DoDelete()) {
    ;
  }
}

bool AllocationStressThread::RemoveRandomAllocation(
    std::pair<const void*, AllocationRecord>* output) {
  if (allocated_pts_.empty()) {
    return false;
  }

  // Select a random pointer to delete.
  int idx = std::rand() % allocated_pts_.size();
  AllocMap::iterator iter = allocated_pts_.begin();
  while (idx > 0) {
    idx--;
    iter++;
  }
  output->first = iter->first;
  output->second = iter->second;
  allocated_pts_.erase(iter);
  return true;
}

bool AllocationStressThread::DoDelete() {
  NoMemTracking no_memory_tracking_in_this_scope;
  ++do_delete_counter_;

  std::pair<const void*, AllocationRecord> alloc;
  if (!RemoveRandomAllocation(&alloc)) {
    return false;
  }

  const void* ptr = alloc.first;
  const AllocationRecord expected_alloc_record = alloc.second;

  TRACK_MEMORY_SCOPE_DYNAMIC(unique_name_.c_str());
  AllocationGroup* current_group = memory_tracker_->PeekAllocationGroup();

  // Expect that the name of the current allocation group name is the same as
  // what we expect.
  if (current_group->name() != unique_name_) {
    NoMemTracking no_memory_tracking_in_this_scope;
    ADD_FAILURE_AT(__FILE__, __LINE__) << " " << current_group->name()
                                       << " != " << unique_name_;
  }

  MemoryTrackerImpl::AllocationMapType* internal_alloc_map =
      memory_tracker_->pointer_map();

  AllocationRecord existing_alloc_record;

  const bool found_existing_record =
      memory_tracker_->GetMemoryTracking(ptr, &existing_alloc_record);

  if (!found_existing_record) {
    ADD_FAILURE_AT(__FILE__, __LINE__)
        << "expected to find existing record, but did not";
  } else if (current_group != existing_alloc_record.allocation_group) {
    ADD_FAILURE_AT(__FILE__, __LINE__)
        << "group allocation mismatch: " << current_group->name()
        << " != " << existing_alloc_record.allocation_group->name() << "\n";
  }
  SbMemoryDeallocate(const_cast<void*>(ptr));
  return true;
}

void AllocationStressThread::DoMalloc() {
  ++do_malloc_counter_;
  if (allocated_pts_.size() > 10000) {
    return;
  }

  TRACK_MEMORY_SCOPE_DYNAMIC(unique_name_.c_str());
  AllocationGroup* current_group = memory_tracker_->PeekAllocationGroup();

  // Sanity check, make sure that the current_group name is the same as
  // our unique name.
  if (current_group->name() != unique_name_) {
    NoMemTracking no_tracking_in_scope;
    ADD_FAILURE_AT(__FILE__, __LINE__) << " " << current_group->name()
                                       << " != " << unique_name_;
  }

  if (!memory_tracker_->IsMemoryTrackingEnabled()) {
    NoMemTracking no_tracking_in_scope;
    ADD_FAILURE_AT(__FILE__, __LINE__)
        << " memory tracking state was disabled.";
  }

  const int alloc_size = std::rand() % 100 + 8;

  void* memory = SbMemoryAllocate(alloc_size);

  AllocationRecord record;
  bool found = memory_tracker_->GetMemoryTracking(memory, &record);
  if (!found) {
    NoMemTracking no_tracking_in_scope;
    ADD_FAILURE_AT(__FILE__, __LINE__)
        << "Violated expectation, malloc counter: " << do_malloc_counter_;
  }
  AllocMap::iterator found_it = allocated_pts_.find(memory);

  if (found_it != allocated_pts_.end()) {
    NoMemTracking no_tracking_in_scope;
    ADD_FAILURE_AT(__FILE__, __LINE__)
        << "This pointer should not be in the map.";
  }

  NoMemTracking no_tracking_in_scope;
  allocated_pts_[memory] = AllocationRecord(alloc_size, current_group);
}

}  // namespace
}  // namespace analytics
}  // namespace nb
