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

#include "nb/analytics/memory_tracker.h"
#include "nb/analytics/memory_tracker_helpers.h"
#include "nb/memory_scope.h"
#include "nb/scoped_ptr.h"
#include "starboard/memory.h"
#include "starboard/memory_reporter.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace analytics {
namespace {

MemoryTracker* s_memory_tracker_ = NULL;

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

#define ASSERT_TRUE_NO_TRACKING(A)                  \
  {                                                 \
    NoMemTracking no_memory_tracking_in_this_scope; \
    ASSERT_TRUE(A);                                 \
  }

///////////////////////////////////////////////////////////////////////////////
// Framework which initializes the MemoryTracker once and installs it
// for the first test and the removes the MemoryTracker after the
// the last test finishes.
class MemoryTrackerTest : public ::testing::Test {
 public:
  MemoryTrackerTest() {}

  MemoryTracker* memory_tracker() { return s_memory_tracker_; }

  bool GetAllocRecord(void* alloc_memory, AllocationRecord* output) {
    return memory_tracker()->GetMemoryTracking(alloc_memory, output);
  }

  int64_t TotalNumberOfAllocations() {
    return memory_tracker()->GetTotalNumberOfAllocations();
  }

  int64_t TotalAllocationBytes() {
    return memory_tracker()->GetTotalAllocationBytes();
  }

  bool MemoryTrackerEnabled() const { return s_memory_tracker_enabled_; }

 protected:
  static void SetUpTestCase() {
    s_memory_tracker_ = MemoryTracker::Get();
    s_memory_tracker_enabled_ = s_memory_tracker_->InstallGlobalTrackingHooks();
  }
  static void TearDownTestCase() {
    s_memory_tracker_->RemoveGlobalTrackingHooks();
    // Give time for all threads to sync up to the fact that the memory tracker
    // has been removed.
    SbThreadSleep(50 * kSbTimeMillisecond);
  }
  static bool s_memory_tracker_enabled_;
};
bool MemoryTrackerTest::s_memory_tracker_enabled_ = false;

///////////////////////////////////////////////////////////////////////////////
class FindAllocationVisitor : public AllocationVisitor {
 public:
  FindAllocationVisitor() : found_(false), memory_to_find_(NULL) {}

  bool found() const { return found_; }
  void set_found(bool val) { found_ = val; }
  void set_memory_to_find(const void* memory) {
    memory_to_find_ = memory;
    found_ = false;
  }

  virtual bool Visit(const void* memory, const AllocationRecord& alloc_record) {
    if (memory_to_find_ == memory) {
      found_ = true;
      return false;
    }
    return true;
  }

 private:
  bool found_;
  const void* memory_to_find_;
};

///////////////////////////////////////////////////////////////////////////////
TEST_F(MemoryTrackerTest, MacrosScopedObject) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }

  scoped_ptr<int> alloc_a, alloc_b;
  {
    TRACK_MEMORY_SCOPE("MemoryTrackerTest-ScopeA");
    alloc_a.reset(new int());
    {
      TRACK_MEMORY_SCOPE("MemoryTrackerTest-ScopeB");
      alloc_b.reset(new int());
    }
  }

  // Now test that the allocations now exist in the memory tracker.
  AllocationRecord alloc_record_a, alloc_record_b;
  // Expect that the allocations exist and that the AllocRecords are written
  // with the allocation information.
  EXPECT_TRUE_NO_TRACKING(
      memory_tracker()->GetMemoryTracking(alloc_a.get(), &alloc_record_a));
  EXPECT_TRUE_NO_TRACKING(
      memory_tracker()->GetMemoryTracking(alloc_b.get(), &alloc_record_b));

  // Sanity test that the allocations are non-null.

  const AllocationGroup* group_a = alloc_record_a.allocation_group;
  const AllocationGroup* group_b = alloc_record_b.allocation_group;
  ASSERT_TRUE_NO_TRACKING(group_a);
  ASSERT_TRUE_NO_TRACKING(group_b);

  EXPECT_EQ_NO_TRACKING(group_a->name(),
                        std::string("MemoryTrackerTest-ScopeA"));
  EXPECT_EQ_NO_TRACKING(group_b->name(),
                        std::string("MemoryTrackerTest-ScopeB"));

  // When the allocation is returned to the free store then it's expected that
  // the memory tracker will indicate that the allocation no longer exists.
  alloc_a.reset();
  alloc_b.reset();

  EXPECT_FALSE_NO_TRACKING(
      memory_tracker()->GetMemoryTracking(alloc_a.get(), &alloc_record_a));
  EXPECT_FALSE_NO_TRACKING(
      memory_tracker()->GetMemoryTracking(alloc_b.get(), &alloc_record_b));

  int32_t num_allocations = -1;
  int64_t allocation_bytes = -1;

  group_a->GetAggregateStats(&num_allocations, &allocation_bytes);
  EXPECT_EQ_NO_TRACKING(0, num_allocations);
  EXPECT_EQ_NO_TRACKING(0, allocation_bytes);

  group_b->GetAggregateStats(&num_allocations, &allocation_bytes);
  EXPECT_EQ_NO_TRACKING(0, num_allocations);
  EXPECT_EQ_NO_TRACKING(0, allocation_bytes);
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(MemoryTrackerTest, Visitor) {
  // Memory tracker is not enabled for this build.
  if (!MemoryTrackerEnabled()) {
    return;
  }

  FindAllocationVisitor visitor;

  scoped_ptr<int> alloc_a;
  {
    TRACK_MEMORY_SCOPE("MemoryTrackerTest-ScopeA");

    alloc_a.reset(new int());
    visitor.set_memory_to_find(alloc_a.get());
    memory_tracker()->Accept(&visitor);
    EXPECT_TRUE_NO_TRACKING(visitor.found());

    alloc_a.reset(NULL);
    visitor.set_found(false);
    memory_tracker()->Accept(&visitor);
    EXPECT_FALSE_NO_TRACKING(visitor.found());
  }
}

}  // namespace
}  // namespace analytics
}  // namespace nb
