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

#include "nb/memory_scope.h"
#include "nb/thread_local_object.h"
#include "starboard/common/mutex.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace {

bool StarboardAllowsMemoryTracking() {
#if defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
  return true;
#else
  return false;
#endif
}

// This is a memory scope reporter that is compatible
// with the MemoryScopeReporter.
class TestMemoryScopeReporter {
 public:
  typedef std::vector<NbMemoryScopeInfo*> MemoryScopeVector;

  TestMemoryScopeReporter() {
    memory_scope_reporter_ = CreateMemoryScopeReporter();
  }

  NbMemoryScopeReporter* memory_scope_reporter() {
    return &memory_scope_reporter_;
  }

  MemoryScopeVector* stack_thread_local() { return stack_tlo_.GetOrCreate(); }

  void OnPushMemoryScope(NbMemoryScopeInfo* memory_scope) {
    stack_thread_local()->push_back(memory_scope);
  }

  void OnPopMemoryScope() {
    MemoryScopeVector* stack = stack_thread_local();
    if (!stack->empty()) {
      stack->pop_back();
    } else {
      ADD_FAILURE_AT(__FILE__, __LINE__)
          << " stack was empty and could not be popped.";
    }
  }

 private:
  static void OnPushMemoryScopeCallback(void* context,
                                        NbMemoryScopeInfo* info) {
    TestMemoryScopeReporter* t = static_cast<TestMemoryScopeReporter*>(context);
    t->OnPushMemoryScope(info);
  }

  static void OnPopMemoryScopeCallback(void* context) {
    TestMemoryScopeReporter* t = static_cast<TestMemoryScopeReporter*>(context);
    t->OnPopMemoryScope();
  }

  NbMemoryScopeReporter CreateMemoryScopeReporter() {
    NbMemoryScopeReporter reporter = {OnPushMemoryScopeCallback,
                                      OnPopMemoryScopeCallback, this};
    return reporter;
  }

  NbMemoryScopeReporter memory_scope_reporter_;
  ThreadLocalObject<MemoryScopeVector> stack_tlo_;
};

// A test framework for testing the Pushing & popping memory scopes.
// The key feature here is that reporter is setup on the first test
// instance and torn down after the last test has run.
class MemoryScopeReportingTest : public ::testing::Test {
 public:
  TestMemoryScopeReporter* test_memory_reporter() { return s_reporter_; }

  bool reporting_enabled() const { return s_reporter_enabled_; }

 protected:
  static void SetUpTestCase() {
    if (!s_reporter_) {
      s_reporter_ = new TestMemoryScopeReporter;
    }
    s_reporter_enabled_ =
        NbSetMemoryScopeReporter(s_reporter_->memory_scope_reporter());

    EXPECT_EQ(StarboardAllowsMemoryTracking(), s_reporter_enabled_)
        << "Expected the memory scope reporter to be enabled whenever "
           "starboard memory tracking is allowed.";
  }

  static void TearDownTestCase() {
    // The reporter itself is not deleted because other threads could
    // be traversing through it's data structures. It's better just to leave
    // the object alive for the purposes of this unit test and set the pointer
    // to NULL.
    // This is done in order to make the MemoryScopeReport object lock free.
    // This increases performance and reduces complexity of design.
    NbSetMemoryScopeReporter(NULL);
  }

  // Per test setup.
  virtual void SetUp() {
    test_memory_reporter()->stack_thread_local()->clear();
  }

  static TestMemoryScopeReporter* s_reporter_;
  static bool s_reporter_enabled_;
};
TestMemoryScopeReporter* MemoryScopeReportingTest::s_reporter_ = NULL;
bool MemoryScopeReportingTest::s_reporter_enabled_;

///////////////////////////////////////////////////////////////////////////////
// TESTS.
// There are two sets of tests: POSITIVE and NEGATIVE.
//  The positive tests are active when STARBOARD_ALLOWS_MEMORY_TRACKING is
//  defined and test that memory tracking is enabled.
//  NEGATIVE tests ensure that tracking is disabled when
//  STARBOARD_ALLOWS_MEMORY_TRACKING is not defined.
// When adding new tests:
//  POSITIVE tests are named normally.
//  NEGATIVE tests are named with "No" prefixed to the beginning.
//  Example:
//   TEST_F(MemoryScopeReportingTest, PushPop) <--- POSITIVE test.
//   TEST_F(MemoryScopeReportingTest, NoPushPop) <- NEGATIVE test.
//  All positive & negative tests are grouped together.
///////////////////////////////////////////////////////////////////////////////

#if defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
// These are POSITIVE tests, which test the expectation that when the define
// STARBOARD_ALLOWS_MEMORY_TRACKING is active that the memory scope reporter
// will receive memory scope notifications.

// Tests the assumption that the SbMemoryAllocate and SbMemoryDeallocate
// will report memory allocations.
TEST_F(MemoryScopeReportingTest, PushPop) {
  ASSERT_TRUE(reporting_enabled());
  const int line_number = __LINE__;
  const char* file_name = __FILE__;
  const char* function_name = __FUNCTION__;
  NbMemoryScopeInfo info = {0,              // Cached value (null).
                            "Javascript",   // Name of the memory scope.
                            file_name,      // Filename that invoked this.
                            line_number,    // Line number.
                            function_name,  // Function name.
                            true};          // true allows caching.

  NbPushMemoryScope(&info);

  ASSERT_FALSE(test_memory_reporter()->stack_thread_local()->empty());
  NbMemoryScopeInfo* info_ptr =
      test_memory_reporter()->stack_thread_local()->front();

  EXPECT_EQ(&info, info_ptr);
  EXPECT_STREQ(info.file_name_, file_name);
  EXPECT_STREQ(info.function_name_, function_name);
  EXPECT_EQ(info.line_number_, line_number);

  NbPopMemoryScope();
  EXPECT_TRUE(test_memory_reporter()->stack_thread_local()->empty());
}

// Tests the expectation that the memory reporting macros will
// push/pop memory regions and will also correctly bind to the
// file, linenumber and also the function name.
TEST_F(MemoryScopeReportingTest, Macros) {
  ASSERT_TRUE(reporting_enabled());
  // There should be no leftover stack objects.
  EXPECT_TRUE(test_memory_reporter()->stack_thread_local()->empty());
  {
    const int line_before = __LINE__;
    TRACK_MEMORY_SCOPE("TestMemoryScope");
    const int predicted_line = line_before + 1;

    NbMemoryScopeInfo* info_ptr =
        test_memory_reporter()->stack_thread_local()->front();

    // TRACK_MEMORY_SCOPE is defined to allow caching.
    EXPECT_EQ(true, info_ptr->allows_caching_);

    // The cached_handle_ is not mutated by TestMemoryScopeReporter so
    // therefore it should be the default value of 0.
    EXPECT_EQ(0, info_ptr->cached_handle_);

    EXPECT_STREQ("TestMemoryScope", info_ptr->memory_scope_name_);
    EXPECT_STREQ(__FILE__, info_ptr->file_name_);
    EXPECT_EQ(predicted_line, info_ptr->line_number_);
    EXPECT_STREQ(__FUNCTION__, info_ptr->function_name_);
  }
  // Expect that the stack object is now empty again.
  EXPECT_TRUE(test_memory_reporter()->stack_thread_local()->empty());
}

#else  // !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
// These are NEGATIVE tests, which test the expectation that when the
// STARBOARD_ALLOWS_MEMORY_TRACKING is undefined that the memory scope reprter
// does not receive memory scope notifications.

// Tests the expectation that push pop does not send notifications to the
// reporter when disabled.
TEST_F(MemoryScopeReportingTest, NoPushPop) {
  ASSERT_FALSE(reporting_enabled());
  const int line_number = __LINE__;
  const char* file_name = __FILE__;
  const char* function_name = __FUNCTION__;
  NbMemoryScopeInfo info = {0,              // Cached value (null).
                            "Javascript",   // Name of the memory scope.
                            file_name,      // Filename that invoked this.
                            line_number,    // Line number.
                            function_name,  // Function name.
                            true};          // true allows caching.

  NbPushMemoryScope(&info);
  ASSERT_TRUE(test_memory_reporter()->stack_thread_local()->empty());
  NbPopMemoryScope();
  EXPECT_TRUE(test_memory_reporter()->stack_thread_local()->empty());
}

// Tests the expectation that the memory reporting macros are disabled when
// memory tracking is not allowed.
TEST_F(MemoryScopeReportingTest, NoMacros) {
  ASSERT_FALSE(reporting_enabled());
  // Test that the macros do nothing when memory reporting has been
  // disabled.
  TRACK_MEMORY_SCOPE("InternalMemoryRegion");
  ASSERT_TRUE(test_memory_reporter()->stack_thread_local()->empty())
      << "Memory reporting received notifications when it should be disabled.";
}
#endif

}  // namespace.
}  // namespace nb.
