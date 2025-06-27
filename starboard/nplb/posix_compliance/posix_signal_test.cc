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
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// --- Globals for Signal Handlers ---

// A global, signal-safe flag to indicate a signal was received.
// It must be volatile and sig_atomic_t to be safely accessed in a handler.
volatile sig_atomic_t g_signal_received = 0;

// A global file descriptor for the pipe used for reliable signal-event
// notification. A static global is used so the C-style signal handler can
// access it. It is set up and torn down by the test fixture.
static int g_signal_pipe_write_fd = -1;

// --- Signal Handlers ---

// A simple signal handler that sets a global flag.
void TestSignalHandler(int signum) {
  g_signal_received = signum;
}

// An async-signal-safe handler that writes the signal number to a pipe.
// This allows the main thread to wait on the pipe's read end, providing a
// reliable way to detect that the handler has executed.
void PipeWritingSignalHandler(int signum) {
  char signal_byte = static_cast<char>(signum);
  // This write is async-signal-safe.
  // The return value is intentionally ignored here.
  // Avoid using library functions or complex logic in a handler.
  // If write fails, the test waiting on the pipe will time out.
  std::ignore = write(g_signal_pipe_write_fd, &signal_byte, 1);
}

// --- Threading Structures and Functions ---

// A structure to pass arguments to a new thread, avoiding global variables
// for coordination.
struct ThreadArgs {
  pthread_mutex_t* mutex;
  pthread_cond_t* cond;
  bool* thread_ready;
};

// A thread entry point that uses pause() to wait for a signal.
// It uses a condition variable to notify the main thread when it's ready.
void* WaitForSigusr1(void* arg) {
  ThreadArgs* thread_args = static_cast<ThreadArgs*>(arg);

  // Install a handler for SIGUSR1.
  struct sigaction sa = {};
  sa.sa_handler = TestSignalHandler;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGUSR1, &sa, nullptr) != 0) {
    return reinterpret_cast<void*>(-1);  // Indicate error
  }

  // Signal the main thread that the handler is installed and we are ready.
  pthread_mutex_lock(thread_args->mutex);
  *thread_args->thread_ready = true;
  pthread_cond_signal(thread_args->cond);
  pthread_mutex_unlock(thread_args->mutex);

  // Wait for a signal. pause() returns when a signal handler has returned.
  pause();

  return nullptr;
}

// --- Test Fixture ---

// Test fixture for POSIX signal tests. It handles saving and restoring
// original signal dispositions and sets up synchronization primitives for
// reliable signal and thread testing.
class PosixSignalTest : public ::testing::Test {
 protected:
  // Helper to set up a non-blocking pipe for signal communication.
  void CreateNonBlockingPipe() {
    ASSERT_EQ(pipe(pipe_fds_), 0) << "pipe() failed: " << strerror(errno);
    // Set the write end to non-blocking.
    int flags = fcntl(pipe_fds_[1], F_GETFL);
    ASSERT_NE(flags, -1);
    ASSERT_NE(fcntl(pipe_fds_[1], F_SETFL, flags | O_NONBLOCK), -1);
    g_signal_pipe_write_fd = pipe_fds_[1];
  }

  // Helper to set up an epoll instance to listen on the pipe's read end.
  void CreateEpollListener() {
    epoll_fd_ = epoll_create1(0);
    ASSERT_NE(epoll_fd_, -1) << "epoll_create1() failed: " << strerror(errno);

    struct epoll_event event = {};
    event.events = EPOLLIN;
    event.data.fd = pipe_fds_[0];
    ASSERT_EQ(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, pipe_fds_[0], &event), 0)
        << "epoll_ctl() failed: " << strerror(errno);
  }

  void SetUp() override {
    // Reset the global flag before each test.
    g_signal_received = 0;

    // --- Set up pipe and epoll for reliable signal waiting ---
    CreateNonBlockingPipe();
    CreateEpollListener();

    // --- Set up thread coordination primitives ---
    ASSERT_EQ(pthread_mutex_init(&mutex_, nullptr), 0);
    ASSERT_EQ(pthread_cond_init(&cond_, nullptr), 0);
    thread_ready_ = false;

    // --- Save original signal actions ---
    ASSERT_EQ(sigaction(SIGUSR1, nullptr, &original_sa_usr1_), 0);
    ASSERT_EQ(sigaction(SIGUSR2, nullptr, &original_sa_usr2_), 0);
    ASSERT_EQ(sigaction(SIGALRM, nullptr, &original_sa_alrm_), 0);
  }

  void TearDown() override {
    // --- Clean up file descriptors ---
    if (pipe_fds_[0] != -1) {
      close(pipe_fds_[0]);
    }
    if (pipe_fds_[1] != -1) {
      close(pipe_fds_[1]);
    }
    if (epoll_fd_ != -1) {
      close(epoll_fd_);
    }
    g_signal_pipe_write_fd = -1;

    // --- Clean up thread primitives ---
    pthread_mutex_destroy(&mutex_);
    pthread_cond_destroy(&cond_);

    // --- Restore original signal actions ---
    EXPECT_EQ(sigaction(SIGUSR1, &original_sa_usr1_, nullptr), 0);
    EXPECT_EQ(sigaction(SIGUSR2, &original_sa_usr2_, nullptr), 0);
    EXPECT_EQ(sigaction(SIGALRM, &original_sa_alrm_, nullptr), 0);
  }

  // Waits for a signal to be delivered using epoll on a pipe.
  // Returns true if the signal was received, false on timeout.
  // The received signal number is written to the out-parameter `signum`.
  bool WaitForSignalWithTimeout(int* signum, int timeout_ms) {
    struct epoll_event event;
    int num_events = epoll_wait(epoll_fd_, &event, 1, timeout_ms);

    if (num_events <= 0) {
      return false;  // Timeout or error
    }

    char buffer;
    if (read(pipe_fds_[0], &buffer, 1) == 1) {
      *signum = buffer;
      return true;
    }
    return false;
  }

  // Fixture members
  int pipe_fds_[2] = {-1, -1};
  int epoll_fd_ = -1;
  pthread_mutex_t mutex_;
  pthread_cond_t cond_;
  bool thread_ready_ = false;

  struct sigaction original_sa_usr1_;
  struct sigaction original_sa_usr2_;
  struct sigaction original_sa_alrm_;
};

// --- Signal Handler Registration and Delivery Tests ---

TEST_F(PosixSignalTest, SignalLegacyHandler) {
  sighandler_t old_handler = signal(SIGUSR1, PipeWritingSignalHandler);
  ASSERT_NE(old_handler, SIG_ERR);

  ASSERT_EQ(raise(SIGUSR1), 0);

  int received_signal = 0;
  EXPECT_TRUE(WaitForSignalWithTimeout(&received_signal, 1000));
  EXPECT_EQ(received_signal, SIGUSR1);
}

TEST_F(PosixSignalTest, SigactionModernHandler) {
  struct sigaction sa = {};
  sa.sa_handler = PipeWritingSignalHandler;
  sigemptyset(&sa.sa_mask);
  ASSERT_EQ(sigaction(SIGUSR2, &sa, nullptr), 0);

  ASSERT_EQ(raise(SIGUSR2), 0);

  int received_signal = 0;
  EXPECT_TRUE(WaitForSignalWithTimeout(&received_signal, 1000));
  EXPECT_EQ(received_signal, SIGUSR2);
}

TEST_F(PosixSignalTest, KillSendsSignalToSelf) {
  struct sigaction sa = {};
  sa.sa_handler = PipeWritingSignalHandler;
  sigemptyset(&sa.sa_mask);
  ASSERT_EQ(sigaction(SIGUSR1, &sa, nullptr), 0);

  ASSERT_EQ(kill(getpid(), SIGUSR1), 0);

  int received_signal = 0;
  EXPECT_TRUE(WaitForSignalWithTimeout(&received_signal, 1000));
  EXPECT_EQ(received_signal, SIGUSR1);
}

TEST_F(PosixSignalTest, PauseIsInterruptedBySignal) {
  // This test correctly uses pause() to wait, so no sleep is needed.
  // It verifies that pause() is interrupted as expected.
  struct sigaction sa = {};
  sa.sa_handler = TestSignalHandler;  // Uses the simple flag-setting handler
  sigemptyset(&sa.sa_mask);
  ASSERT_EQ(sigaction(SIGALRM, &sa, nullptr), 0);

  alarm(1);
  pause();  // Block until the alarm signal is delivered.

  EXPECT_EQ(g_signal_received, SIGALRM);
  alarm(0);  // Cancel any pending alarm.
}

// --- Signal Masking Test ---

TEST_F(PosixSignalTest, SigprocmaskBlocksAndUnblocksSignal) {
  struct sigaction sa = {};
  sa.sa_handler = PipeWritingSignalHandler;
  sigemptyset(&sa.sa_mask);
  ASSERT_EQ(sigaction(SIGUSR1, &sa, nullptr), 0);

  sigset_t block_mask, old_mask;
  sigemptyset(&block_mask);
  sigaddset(&block_mask, SIGUSR1);

  // Block SIGUSR1.
  ASSERT_EQ(sigprocmask(SIG_BLOCK, &block_mask, &old_mask), 0);

  // Raise SIGUSR1. It should be pending.
  ASSERT_EQ(raise(SIGUSR1), 0);

  // Verify the signal has not been received yet by waiting for a short time.
  int received_signal = 0;
  EXPECT_FALSE(WaitForSignalWithTimeout(&received_signal, 50))
      << "Signal was received while blocked.";
  EXPECT_EQ(received_signal, 0);

  // Unblock SIGUSR1. The pending signal should now be delivered.
  ASSERT_EQ(sigprocmask(SIG_SETMASK, &old_mask, nullptr), 0);

  // Verify the signal was finally received.
  EXPECT_TRUE(WaitForSignalWithTimeout(&received_signal, 1000))
      << "Signal was not received after unblocking.";
  EXPECT_EQ(received_signal, SIGUSR1);
}

TEST_F(PosixSignalTest, PthreadSigmaskBlocksAndUnblocksSignal) {
  struct sigaction sa = {};
  sa.sa_handler = PipeWritingSignalHandler;
  sigemptyset(&sa.sa_mask);
  ASSERT_EQ(sigaction(SIGUSR2, &sa, nullptr), 0);

  sigset_t block_mask, old_mask;
  sigemptyset(&block_mask);
  sigaddset(&block_mask, SIGUSR2);

  // Block SIGUSR2 for the current thread.
  ASSERT_EQ(pthread_sigmask(SIG_BLOCK, &block_mask, &old_mask), 0);

  // Raise SIGUSR2. It should be pending for this thread.
  ASSERT_EQ(raise(SIGUSR2), 0);

  // Verify the signal has not been received yet.
  int received_signal = 0;
  EXPECT_FALSE(WaitForSignalWithTimeout(&received_signal, 50));

  // Unblock SIGUSR2. The pending signal should now be delivered.
  ASSERT_EQ(pthread_sigmask(SIG_SETMASK, &old_mask, nullptr), 0);

  // Verify the signal was finally received.
  EXPECT_TRUE(WaitForSignalWithTimeout(&received_signal, 1000));
  EXPECT_EQ(received_signal, SIGUSR2);
}

// --- Multi-threaded Signal Tests ---

TEST_F(PosixSignalTest, PthreadKillSendsSignalToSpecificThread) {
  // Use a condition variable for reliable thread readiness notification.
  ThreadArgs args = {&mutex_, &cond_, &thread_ready_};
  pthread_t thread;
  ASSERT_EQ(pthread_create(&thread, nullptr, WaitForSigusr1, &args), 0);

  // Wait until the thread has signaled that it's ready.
  pthread_mutex_lock(&mutex_);
  while (!thread_ready_) {
    // Use a timed wait to prevent hanging indefinitely if the thread fails.
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 2;  // 2-second timeout
    int result = pthread_cond_timedwait(&cond_, &mutex_, &timeout);
    ASSERT_EQ(result, 0) << "Timed out waiting for thread to become ready.";
  }
  pthread_mutex_unlock(&mutex_);

  // Send SIGUSR1 specifically to the new thread.
  int ret = pthread_kill(thread, SIGUSR1);
  EXPECT_EQ(ret, 0) << "pthread_kill() failed: " << strerror(ret);

  // Wait for the thread to finish.
  pthread_join(thread, nullptr);

  // Check that the thread's handler was called.
  EXPECT_EQ(g_signal_received, SIGUSR1);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
