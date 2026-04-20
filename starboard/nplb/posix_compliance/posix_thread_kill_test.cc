// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// Global flag to be set by the signal handler.
// Needs to be accessible by the static signal handler function.
std::atomic<bool> g_signal_received_flag(false);

void SignalHandler(int signum) {
  if (signum == SIGUSR1) {
    g_signal_received_flag.store(true, std::memory_order_relaxed);
  }
}

// Thread function that sets up a signal handler and waits.
void* SignalWaitingThreadFunc(void* arg) {
  g_signal_received_flag.store(false,
                               std::memory_order_relaxed);  // Reset flag.

  if (signal(SIGUSR1, SignalHandler) == SIG_ERR) {
    return reinterpret_cast<void*>(1);  // Indicate error.
  }

  // Wait for the signal by polling the flag.
  // In a real app, use sigwait, sigsuspend, or condition variables.
  for (int i = 0; i < 200; ++i) {  // Max ~2 seconds wait
    if (g_signal_received_flag.load(std::memory_order_relaxed)) {
      break;
    }
    usleep(10000);  // 10ms
  }
  return nullptr;
}

// Simple thread function that does nothing for a short while.
static void* NopThreadFunc(void* arg) {
  usleep(50000);  // Sleep for 50ms.
  return nullptr;
}

class PosixThreadKillTest : public ::testing::Test {
 protected:
  sigset_t original_mask_;

  void SetUp() override {
    // Make sure the signal is unblocked.
    sigset_t unblock_mask;
    sigemptyset(&unblock_mask);
    sigaddset(&unblock_mask, SIGUSR1);

    ASSERT_EQ(sigprocmask(SIG_UNBLOCK, &unblock_mask, &original_mask_), 0);
  }

  void TearDown() override {
    // Restore the original mask.
    ASSERT_EQ(sigprocmask(SIG_SETMASK, &original_mask_, nullptr), 0);
  }
};

TEST_F(PosixThreadKillTest, SendSignalToThread) {
  g_signal_received_flag.store(
      false, std::memory_order_relaxed);  // Ensure flag is reset.
  pthread_t thread_id;
  pthread_attr_t attr;
  int ret = pthread_attr_init(&attr);
  ASSERT_EQ(ret, 0) << "pthread_attr_init failed: " << strerror(ret);
  ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  ASSERT_EQ(ret, 0) << "pthread_attr_setdetachstate failed: " << strerror(ret);

  ret = pthread_create(&thread_id, &attr, SignalWaitingThreadFunc, nullptr);
  ASSERT_EQ(ret, 0) << "pthread_create failed: " << strerror(ret);

  // Give the new thread a moment to set up its signal handler.
  usleep(10000);  // 10ms

  ret = pthread_kill(thread_id, SIGUSR1);
  EXPECT_EQ(ret, 0) << "pthread_kill failed to send SIGUSR1: " << strerror(ret);

  void* thread_result = nullptr;
  ret = pthread_join(thread_id, &thread_result);
  EXPECT_EQ(ret, 0) << "pthread_join failed: " << strerror(ret);

  EXPECT_TRUE(g_signal_received_flag.load(std::memory_order_relaxed))
      << "Signal handler for SIGUSR1 was not invoked in the target thread.";

  ret = pthread_attr_destroy(&attr);
  EXPECT_EQ(ret, 0) << "pthread_attr_destroy failed: " << strerror(ret);
}

TEST_F(PosixThreadKillTest, CheckThreadExistsWithSignalZero) {
  pthread_t thread_id;
  pthread_attr_t attr;
  int ret = pthread_attr_init(&attr);
  ASSERT_EQ(ret, 0) << "pthread_attr_init failed: " << strerror(ret);
  ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  ASSERT_EQ(ret, 0) << "pthread_attr_setdetachstate failed: " << strerror(ret);

  ret = pthread_create(&thread_id, &attr, NopThreadFunc, nullptr);
  ASSERT_EQ(ret, 0) << "pthread_create failed: " << strerror(ret);

  // Give the thread a moment to start.
  usleep(10000);  // 10ms

  ret = pthread_kill(thread_id, 0);  // Send null signal to check existence.
  EXPECT_EQ(ret, 0) << "pthread_kill(thread, 0) failed for an existing thread: "
                    << strerror(ret);

  void* thread_result = nullptr;
  ret = pthread_join(thread_id, &thread_result);
  EXPECT_EQ(ret, 0) << "pthread_join failed: " << strerror(ret);

  ret = pthread_attr_destroy(&attr);
  EXPECT_EQ(ret, 0) << "pthread_attr_destroy failed: " << strerror(ret);
}

TEST_F(PosixThreadKillTest, SendInvalidSignalNumber) {
  pthread_t thread_id;
  pthread_attr_t attr;
  int ret = pthread_attr_init(&attr);
  ASSERT_EQ(ret, 0) << "pthread_attr_init failed: " << strerror(ret);
  ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  ASSERT_EQ(ret, 0) << "pthread_attr_setdetachstate failed: " << strerror(ret);

  ret = pthread_create(&thread_id, &attr, NopThreadFunc, nullptr);
  ASSERT_EQ(ret, 0) << "pthread_create failed: " << strerror(ret);

  // Give the thread a moment to start.
  usleep(10000);  // 10ms

  const int invalid_signal = -1;
  ret = pthread_kill(thread_id, invalid_signal);
  EXPECT_EQ(ret, EINVAL) << "pthread_kill with an invalid signal number did "
                            "not return EINVAL. Got: "
                         << (ret == 0 ? "0 (success)" : strerror(ret)) << " ("
                         << ret << ")";

  void* thread_result = nullptr;
  ret = pthread_join(thread_id, &thread_result);
  EXPECT_EQ(ret, 0) << "pthread_join failed: " << strerror(ret);

  ret = pthread_attr_destroy(&attr);
  EXPECT_EQ(ret, 0) << "pthread_attr_destroy failed: " << strerror(ret);
}

}  // namespace
}  // namespace nplb
