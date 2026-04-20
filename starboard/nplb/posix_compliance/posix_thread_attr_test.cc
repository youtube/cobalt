// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <pthread.h>

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

constexpr int kStackSize = 1 * 1024 * 1024;

TEST(PosixThreadAttrTest, InitAttr) {
  pthread_attr_t attr;
  int ret = pthread_attr_init(&attr);
  EXPECT_EQ(ret, 0) << "pthread_attr_init failed: " << strerror(ret);
  if (ret == 0) {
    ret = pthread_attr_destroy(&attr);
    EXPECT_EQ(ret, 0) << "pthread_attr_destroy failed: " << strerror(ret);
  }
}

TEST(PosixThreadAttrTest, DetachAttr) {
  pthread_attr_t attr;
  int ret = pthread_attr_init(&attr);
  ASSERT_EQ(ret, 0) << "pthread_attr_init failed: " << strerror(ret);

  int detach_state = -1;

  // Get default detach state.
  ret = pthread_attr_getdetachstate(&attr, &detach_state);
  EXPECT_EQ(ret, 0) << "pthread_attr_getdetachstate (default) failed: "
                    << strerror(ret);
  if (ret == 0) {
    EXPECT_EQ(detach_state, PTHREAD_CREATE_JOINABLE);
  }

  // Set detach state to detached.
  ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  EXPECT_EQ(ret, 0) << "pthread_attr_setdetachstate (detached) failed: "
                    << strerror(ret);

  // Get detach state again.
  ret = pthread_attr_getdetachstate(&attr, &detach_state);
  EXPECT_EQ(ret, 0) << "pthread_attr_getdetachstate (detached) failed: "
                    << strerror(ret);
  if (ret == 0) {
    EXPECT_EQ(detach_state, PTHREAD_CREATE_DETACHED);
  }

  ret = pthread_attr_destroy(&attr);
  EXPECT_EQ(ret, 0) << "pthread_attr_destroy failed: " << strerror(ret);
}

TEST(PosixThreadAttrTest, StackSizeAttr) {
  pthread_attr_t attr;
  int ret = pthread_attr_init(&attr);
  ASSERT_EQ(ret, 0) << "pthread_attr_init failed: " << strerror(ret);

  ret = pthread_attr_setstacksize(&attr, kStackSize);
  EXPECT_EQ(ret, 0) << "pthread_attr_setstacksize failed: " << strerror(ret);

  size_t stack_size = 0;
  ret = pthread_attr_getstacksize(&attr, &stack_size);
  EXPECT_EQ(ret, 0) << "pthread_attr_getstacksize failed: " << strerror(ret);
  EXPECT_EQ(static_cast<int>(stack_size), kStackSize);

  ret = pthread_attr_destroy(&attr);
  EXPECT_EQ(ret, 0) << "pthread_attr_destroy failed: " << strerror(ret);
}

// Test for setting and getting both stack address and size.
TEST(PosixThreadAttrTest, StackAddrAndSizeAttr) {
  pthread_attr_t attr;
  std::array<char, kStackSize> stack_buffer;
  void* set_stack_addr = static_cast<void*>(stack_buffer.data());
  void* ret_stack_addr = nullptr;
  size_t ret_stack_size = 0;

  ASSERT_EQ(pthread_attr_init(&attr), 0);

  // Set stack address and size.
  int ret = pthread_attr_setstack(&attr, set_stack_addr, kStackSize);
  ASSERT_EQ(ret, 0) << "pthread_attr_setstack failed: " << strerror(ret);

  // Get stack address and size.
  ret = pthread_attr_getstack(&attr, &ret_stack_addr, &ret_stack_size);
  ASSERT_EQ(ret, 0) << "pthread_attr_getstack failed: " << strerror(ret);

  // Verify retrieved values.
  EXPECT_EQ(ret_stack_addr, set_stack_addr)
      << "Retrieved stack address does not match set address.";

  EXPECT_EQ(ret_stack_size, static_cast<size_t>(kStackSize))
      << "Retrieved stack size (" << ret_stack_size
      << ") should be equal to set size (" << kStackSize << ").";

  ret = pthread_attr_destroy(&attr);
  EXPECT_EQ(ret, 0) << "pthread_attr_destroy failed: " << strerror(ret);
}

// Test for getting and setting the contention scope attribute.
TEST(PosixThreadAttrTest, ScopeAttr) {
  pthread_attr_t attr;
  int ret = pthread_attr_init(&attr);
  ASSERT_EQ(ret, 0) << "pthread_attr_init failed: " << strerror(ret);

  int scope = -1;  // Initialize to an invalid value.
  int supported_scope_count = 0;

  // --- Test PTHREAD_SCOPE_PROCESS ---
  const int process_scope = PTHREAD_SCOPE_PROCESS;
  ret = pthread_attr_setscope(&attr, process_scope);

  if (ret == ENOTSUP) {
    // PTHREAD_SCOPE_PROCESS might not be supported (common on Linux).
    SB_DLOG(INFO) << "Setting scope to PTHREAD_SCOPE_PROCESS is not supported.";
  } else {
    supported_scope_count++;
    EXPECT_EQ(ret, 0)
        << "pthread_attr_setscope (to PROCESS) failed unexpectedly: "
        << strerror(ret);

    // If setting succeeded, get the scope back and verify.
    if (ret == 0) {
      scope = -1;  // Reset
      ret = pthread_attr_getscope(&attr, &scope);
      EXPECT_EQ(ret, 0) << "pthread_attr_getscope (after set PROCESS) failed: "
                        << strerror(ret);
      EXPECT_EQ(scope, process_scope);
    }
  }

  // --- Test PTHREAD_SCOPE_SYSTEM ---
  const int system_scope = PTHREAD_SCOPE_SYSTEM;
  ret = pthread_attr_setscope(&attr, system_scope);

  if (ret == ENOTSUP) {
    // PTHREAD_SCOPE_SYSTEM might not be supported.
    SB_DLOG(INFO) << "Setting scope to PTHREAD_SCOPE_SYSTEM is not supported.";
  } else {
    supported_scope_count++;
    EXPECT_EQ(ret, 0) << "pthread_attr_setscope (to SYSTEM) failed: "
                      << strerror(ret);

    // If setting succeeded, get the scope back and verify.
    if (ret == 0) {
      scope = -1;  // Reset
      ret = pthread_attr_getscope(&attr, &scope);
      EXPECT_EQ(ret, 0) << "pthread_attr_getscope (after set SYSTEM) failed: "
                        << strerror(ret);
      EXPECT_EQ(scope, system_scope);
    }
  }

  // System should support at least one of PTHREAD_SCOPE_PROCESS or
  // PTHREAD_SCOPE_SYSTEM.
  EXPECT_GE(supported_scope_count, 1) << "Neither PTHREAD_SCOPE_PROCESS nor "
                                         "PTHREAD_SCOPE_SYSTEM are supported.";

  ret = pthread_attr_destroy(&attr);
  EXPECT_EQ(ret, 0) << "pthread_attr_destroy failed: " << strerror(ret);
}

// Test for getting and setting the scheduling policy attribute.
TEST(PosixThreadAttrTest, SchedPolicyAttr) {
  pthread_attr_t attr;
  int ret = pthread_attr_init(&attr);
  ASSERT_EQ(ret, 0) << "pthread_attr_init failed: " << strerror(ret);

  // Helper lambda to test setting and getting a specific policy.
  auto test_policy = [&](int policy_to_set, const char* policy_name) {
    const int set_ret = pthread_attr_setschedpolicy(&attr, policy_to_set);
    EXPECT_EQ(set_ret, 0) << "pthread_attr_setschedpolicy (to " << policy_name
                          << ") failed unexpectedly: " << strerror(set_ret);
    if (set_ret != 0) {
      return;
    }
    int policy = -1;  // Initialize to an invalid value.
    const int get_ret = pthread_attr_getschedpolicy(&attr, &policy);
    EXPECT_EQ(get_ret, 0) << "pthread_attr_getschedpolicy (after set "
                          << policy_name << ") failed: " << strerror(get_ret);
    if (get_ret == 0) {
      EXPECT_EQ(policy, policy_to_set);
    }
  };

  test_policy(SCHED_OTHER, "SCHED_OTHER");

  test_policy(SCHED_FIFO, "SCHED_FIFO");

  test_policy(SCHED_RR, "SCHED_RR");

  // Destroy attributes.
  ret = pthread_attr_destroy(&attr);
  EXPECT_EQ(ret, 0) << "pthread_attr_destroy failed: " << strerror(ret);
}

// Simple thread function that does nothing.
static void* NopThreadFunc(void* arg) {
  // A very short sleep can sometimes help ensure the thread is fully "live".
  usleep(1000);  // Sleep for 1ms.
  return nullptr;
}

TEST(PosixThreadAttrTest, GetAttrNp) {
  pthread_attr_t creation_attrs;
  int ret = pthread_attr_init(&creation_attrs);
  ASSERT_EQ(ret, 0) << "pthread_attr_init (creation_attrs) failed: "
                    << strerror(ret);

  // Set attributes for the thread to be created.
  ret = pthread_attr_setdetachstate(&creation_attrs, PTHREAD_CREATE_JOINABLE);
  ASSERT_EQ(ret, 0)
      << "pthread_attr_setdetachstate (PTHREAD_CREATE_JOINABLE) failed: "
      << strerror(ret);

  // Set stack size.
  ret = pthread_attr_setstacksize(&creation_attrs, kStackSize);
  ASSERT_EQ(ret, 0) << "pthread_attr_setstacksize failed for size "
                    << kStackSize << ": " << strerror(ret);

  pthread_t thread;
  ret = pthread_create(&thread, &creation_attrs, NopThreadFunc, nullptr);
  ASSERT_EQ(ret, 0) << "pthread_create failed: " << strerror(ret);

  pthread_attr_t retrieved_attrs;
  ret = pthread_getattr_np(thread, &retrieved_attrs);
  ASSERT_EQ(ret, 0) << "pthread_getattr_np failed: " << strerror(ret);

  // Verify retrieved attributes:
  // 1. Detach state.
  int detach_state = -1;
  ret = pthread_attr_getdetachstate(&retrieved_attrs, &detach_state);
  EXPECT_EQ(ret, 0) << "pthread_attr_getdetachstate (retrieved_attrs) failed: "
                    << strerror(ret);
  EXPECT_EQ(detach_state, PTHREAD_CREATE_JOINABLE);

  // 2. Stack size.
  size_t stack_size = 0;
  ret = pthread_attr_getstacksize(&retrieved_attrs, &stack_size);
  EXPECT_EQ(ret, 0) << "pthread_attr_getstacksize (retrieved_attrs) failed: "
                    << strerror(ret);
  EXPECT_GE(stack_size, static_cast<size_t>(kStackSize));

  // Clean up.
  void* thread_result;
  ret = pthread_join(thread, &thread_result);
  EXPECT_EQ(ret, 0) << "pthread_join failed: " << strerror(ret);

  ret = pthread_attr_destroy(&creation_attrs);
  EXPECT_EQ(ret, 0) << "pthread_attr_destroy (creation_attrs) failed: "
                    << strerror(ret);

  ret = pthread_attr_destroy(&retrieved_attrs);
  EXPECT_EQ(ret, 0) << "pthread_attr_destroy (retrieved_attrs) failed: "
                    << strerror(ret);
}

TEST(PosixThreadAttrTest, SchedParam) {
  pthread_attr_t creation_attrs;
  int ret = pthread_attr_init(&creation_attrs);
  ASSERT_EQ(ret, 0) << "pthread_attr_init (creation_attrs) failed: "
                    << strerror(ret);

  // Ensure thread is joinable for cleanup.
  ret = pthread_attr_setdetachstate(&creation_attrs, PTHREAD_CREATE_JOINABLE);
  ASSERT_EQ(ret, 0)
      << "pthread_attr_setdetachstate (PTHREAD_CREATE_JOINABLE) failed: "
      << strerror(ret);

  pthread_t target_thread;
  ret = pthread_create(&target_thread, &creation_attrs, NopThreadFunc, nullptr);
  ASSERT_EQ(ret, 0) << "pthread_create (target_thread) failed: "
                    << strerror(ret);

  // Helper lambda to test setting and getting a specific policy and priority.
  auto test_runtime_sched_param = [&](pthread_t th, int policy_to_set,
                                      const char* policy_name) {
    struct sched_param new_param = {};
    if (policy_to_set == SCHED_OTHER) {
      new_param.sched_priority = 0;
    } else {
      new_param.sched_priority = 1;
    }

    const int set_ret = pthread_setschedparam(th, policy_to_set, &new_param);
    if (set_ret == EPERM) {
      SB_DLOG(INFO) << "pthread_setschedparam for policy: " << policy_to_set
                    << " failed due to permissions";
      return;
    } else if (set_ret == ENOTSUP) {
      SB_DLOG(INFO) << "pthread_setschedparam for policy: " << policy_to_set
                    << " is not supported.";
      return;
    } else {
      EXPECT_EQ(set_ret, 0) << "pthread_setschedparam (to " << policy_name
                            << ") failed unexpectedly: " << strerror(set_ret);
      if (set_ret == 0) {
        int current_policy;
        struct sched_param current_param;
        const int get_ret =
            pthread_getschedparam(th, &current_policy, &current_param);
        EXPECT_EQ(get_ret, 0)
            << "pthread_getschedparam (after set " << policy_name
            << ") failed: " << strerror(get_ret);
        EXPECT_EQ(current_policy, policy_to_set);
        EXPECT_EQ(current_param.sched_priority, new_param.sched_priority);
      }
    }
  };

  test_runtime_sched_param(target_thread, SCHED_OTHER, "SCHED_OTHER");

  test_runtime_sched_param(target_thread, SCHED_FIFO, "SCHED_FIFO");

  test_runtime_sched_param(target_thread, SCHED_RR, "SCHED_RR");

  // Clean up.
  void* thread_result;
  ret = pthread_join(target_thread, &thread_result);
  EXPECT_EQ(ret, 0) << "pthread_join (target_thread) failed: " << strerror(ret);

  ret = pthread_attr_destroy(&creation_attrs);
  EXPECT_EQ(ret, 0) << "pthread_attr_destroy (creation_attrs) failed: "
                    << strerror(ret);
}

}  // namespace
}  // namespace nplb
