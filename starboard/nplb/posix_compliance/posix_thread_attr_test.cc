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

namespace starboard {
namespace nplb {
namespace {

constexpr int kStackSize = 4 * 1024 * 1024;

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
  EXPECT_EQ(stack_size, kStackSize);

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

  EXPECT_EQ(ret_stack_size, kStackSize)
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

}  // namespace
}  // namespace nplb
}  // namespace starboard
