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

namespace starboard {
namespace nplb {
namespace {

// A global, signal-safe flag to indicate a signal was received.
// It must be volatile and sig_atomic_t to be safely accessed in a handler.
volatile sig_atomic_t g_signal_received = 0;

// A simple signal handler that sets a global flag.
void TestSignalHandler(int signum) {
  g_signal_received = signum;
}

// Test fixture for POSIX signal tests. It handles saving and restoring the
// original signal dispositions and masks for SIGUSR1 and SIGUSR2 to ensure
// tests are isolated and do not interfere with the test runner's state.
class PosixSignalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Reset the global flag before each test.
    g_signal_received = 0;

    // Save the original signal action for SIGUSR1.
    int ret = sigaction(SIGUSR1, nullptr, &original_sa_usr1_);
    ASSERT_EQ(ret, 0) << "Failed to save original SIGUSR1 action: "
                      << strerror(errno);

    // Save the original signal action for SIGUSR2.
    ret = sigaction(SIGUSR2, nullptr, &original_sa_usr2_);
    ASSERT_EQ(ret, 0) << "Failed to save original SIGUSR2 action: "
                      << strerror(errno);
  }

  void TearDown() override {
    // Restore the original signal action for SIGUSR1.
    int ret = sigaction(SIGUSR1, &original_sa_usr1_, nullptr);
    EXPECT_EQ(ret, 0) << "Failed to restore original SIGUSR1 action: "
                      << strerror(errno);

    // Restore the original signal action for SIGUSR2.
    ret = sigaction(SIGUSR2, &original_sa_usr2_, nullptr);
    EXPECT_EQ(ret, 0) << "Failed to restore original SIGUSR2 action: "
                      << strerror(errno);
  }

  struct sigaction original_sa_usr1_;
  struct sigaction original_sa_usr2_;
};

// --- Signal Set Manipulation Tests ---

TEST_F(PosixSignalTest, SigEmptySetAndAddSet) {
  sigset_t set;
  int ret = sigemptyset(&set);
  EXPECT_EQ(ret, 0) << "sigemptyset failed: " << strerror(errno);

  // After sigemptyset, the set should be empty.
  EXPECT_FALSE(sigismember(&set, SIGUSR1));
  EXPECT_FALSE(sigismember(&set, SIGUSR2));

  ret = sigaddset(&set, SIGUSR1);
  EXPECT_EQ(ret, 0) << "sigaddset failed: " << strerror(errno);

  // Now the set should contain SIGUSR1 but not SIGUSR2.
  EXPECT_TRUE(sigismember(&set, SIGUSR1));
  EXPECT_FALSE(sigismember(&set, SIGUSR2));
}

TEST_F(PosixSignalTest, SigFillSetAndDelSet) {
  sigset_t set;
  int ret = sigfillset(&set);
  EXPECT_EQ(ret, 0) << "sigfillset failed: " << strerror(errno);

  // After sigfillset, the set should contain all signals, including SIGUSR1.
  // Note: Some signals cannot be caught or blocked, but sigismember should
  // still report them as part of a full set.
  EXPECT_TRUE(sigismember(&set, SIGUSR1));

  ret = sigdelset(&set, SIGUSR1);
  EXPECT_EQ(ret, 0) << "sigdelset failed: " << strerror(errno);

  // After deleting, SIGUSR1 should no longer be in the set.
  EXPECT_FALSE(sigismember(&set, SIGUSR1));
}

// --- Signal Handler Registration and Delivery Tests ---

TEST_F(PosixSignalTest, SignalLegacyHandler) {
  // Register the handler using the legacy signal() function.
  // Note: Use of signal() is discouraged in favor of sigaction().
  sighandler_t old_handler = signal(SIGUSR1, TestSignalHandler);
  ASSERT_NE(old_handler, SIG_ERR) << "signal() failed: " << strerror(errno);
  EXPECT_EQ(g_signal_received, 0);

  // Raise the signal to the current thread.
  int ret = raise(SIGUSR1);
  EXPECT_EQ(ret, 0) << "raise() failed: " << strerror(errno);

  // A brief pause to allow signal to be delivered, though it's often sync.
  usleep(1000);

  // Check that our handler was called.
  EXPECT_EQ(g_signal_received, SIGUSR1);

  // Restore the original handler.
  signal(SIGUSR1, old_handler);
}

TEST_F(PosixSignalTest, SigactionModernHandler) {
  struct sigaction sa;
  sa.sa_handler = TestSignalHandler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  // Register the handler using the modern sigaction() function.
  int ret = sigaction(SIGUSR2, &sa, NULL);
  ASSERT_EQ(ret, 0) << "sigaction() failed: " << strerror(errno);
  EXPECT_EQ(g_signal_received, 0);

  // Raise the signal to the current thread.
  ret = raise(SIGUSR2);
  EXPECT_EQ(ret, 0) << "raise() failed: " << strerror(errno);
  usleep(1000);

  // Check that our handler was called.
  EXPECT_EQ(g_signal_received, SIGUSR2);
}

// --- Signal Masking Test ---

TEST_F(PosixSignalTest, SigprocmaskBlocksAndUnblocksSignal) {
  // First, register a handler for SIGUSR1 so we can detect if it's delivered.
  struct sigaction sa;
  sa.sa_handler = TestSignalHandler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  int ret = sigaction(SIGUSR1, &sa, NULL);
  ASSERT_EQ(ret, 0) << "Setup: sigaction() failed: " << strerror(errno);

  // Create a signal set with only SIGUSR1.
  sigset_t block_mask, old_mask;
  sigemptyset(&block_mask);
  sigaddset(&block_mask, SIGUSR1);

  // Block SIGUSR1 and save the current mask.
  ret = sigprocmask(SIG_BLOCK, &block_mask, &old_mask);
  ASSERT_EQ(ret, 0) << "sigprocmask(SIG_BLOCK) failed: " << strerror(errno);

  // Raise SIGUSR1. Since it's blocked, the handler should not run yet.
  ret = raise(SIGUSR1);
  EXPECT_EQ(ret, 0) << "raise() failed: " << strerror(errno);
  usleep(1000);

  // Verify the signal has not been received.
  EXPECT_EQ(g_signal_received, 0) << "Signal was received while blocked.";

  // Unblock SIGUSR1 by restoring the old mask. The pending signal should now
  // be delivered immediately.
  ret = sigprocmask(SIG_SETMASK, &old_mask, NULL);
  EXPECT_EQ(ret, 0) << "sigprocmask(SIG_SETMASK) failed: " << strerror(errno);
  usleep(1000);

  // Verify the signal was finally received.
  EXPECT_EQ(g_signal_received, SIGUSR1)
      << "Signal was not received after unblocking.";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
