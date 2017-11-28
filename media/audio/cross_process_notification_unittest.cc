// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/shared_memory.h"
#include "base/stl_util.h"
#include "base/test/multiprocess_test.h"
#include "base/threading/platform_thread.h"
#include "media/audio/cross_process_notification.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#include <utility>  // NOLINT

namespace {

// Initializes (ctor) and deletes (dtor) two vectors of pairs of
// CrossProcessNotification instances.
class NotificationsOwner {
 public:
  // Attempts to create up to |number_of_pairs| number of pairs. Call size()
  // after construction to find out how many pairs were actually created.
  explicit NotificationsOwner(size_t number_of_pairs) {
    CreateMultiplePairs(number_of_pairs);
  }
  ~NotificationsOwner() {
    STLDeleteElements(&a_);
    STLDeleteElements(&b_);
  }

  size_t size() const {
    DCHECK_EQ(a_.size(), b_.size());
    return a_.size();
  }

  const CrossProcessNotification::Notifications& a() { return a_; }
  const CrossProcessNotification::Notifications& b() { return b_; }

 private:
  void CreateMultiplePairs(size_t count) {
    a_.resize(count);
    b_.resize(count);
    size_t i = 0;
    for (; i < count; ++i) {
      a_[i] = new CrossProcessNotification();
      b_[i] = new CrossProcessNotification();
      if (!CrossProcessNotification::InitializePair(a_[i], b_[i])) {
        LOG(WARNING) << "InitializePair failed at " << i;
        delete a_[i];
        delete b_[i];
        break;
      }
    }
    a_.resize(i);
    b_.resize(i);
  }

  CrossProcessNotification::Notifications a_;
  CrossProcessNotification::Notifications b_;
};

// A simple thread that we'll run two instances of.  Both threads get a pointer
// to the same |shared_data| and use a CrossProcessNotification to control when
// each thread can read/write.
class SingleNotifierWorker : public base::PlatformThread::Delegate {
 public:
  SingleNotifierWorker(size_t* shared_data, size_t repeats,
                       CrossProcessNotification* notifier)
      : shared_data_(shared_data), repeats_(repeats),
        notifier_(notifier) {
  }
  virtual ~SingleNotifierWorker() {}

  // base::PlatformThread::Delegate:
  virtual void ThreadMain() override {
    for (size_t i = 0; i < repeats_; ++i) {
      notifier_->Wait();
      ++(*shared_data_);
      notifier_->Signal();
    }
  }

 private:
  size_t* shared_data_;
  size_t repeats_;
  CrossProcessNotification* notifier_;
  DISALLOW_COPY_AND_ASSIGN(SingleNotifierWorker);
};

// Similar to SingleNotifierWorker, except each instance of this class will
// have >1 instances of CrossProcessNotification to Wait/Signal and an equal
// amount of |shared_data| that the notifiers control access to.
class MultiNotifierWorker : public base::PlatformThread::Delegate {
 public:
  MultiNotifierWorker(size_t* shared_data, size_t repeats,
      const CrossProcessNotification::Notifications* notifiers)
      : shared_data_(shared_data), repeats_(repeats),
        notifiers_(notifiers) {
  }
  virtual ~MultiNotifierWorker() {}

  // base::PlatformThread::Delegate:
  virtual void ThreadMain() override {
    CrossProcessNotification::WaitForMultiple waiter(notifiers_);
    for (size_t i = 0; i < repeats_; ++i) {
      int signaled = waiter.Wait();
      ++shared_data_[signaled];
      (*notifiers_)[signaled]->Signal();
    }
  }

 private:
  size_t* shared_data_;
  size_t repeats_;
  const CrossProcessNotification::Notifications* notifiers_;
  DISALLOW_COPY_AND_ASSIGN(MultiNotifierWorker);
};

// A fixed array of bool flags.  Each flag uses 1 bit.  Use sizeof(FlagArray)
// to determine how much memory you need.  The number of flags will therefore
// be sizeof(FlagArray) * 8.
// We use 'struct' to signify that this structures represents compiler
// independent structured data.  I.e. you must be able to map this class
// to a piece of shared memory of size sizeof(FlagArray) and be able to
// use the class.  No vtables etc.
// TODO(tommi): Move this to its own header when we start using it for signaling
// audio devices.  As is, it's just here for perf comparison against the
// "multiple notifiers" approach.
struct FlagArray {
 public:
  FlagArray() : flags_() {}

  bool is_set(size_t index) const {
    return (flags_[index >> 5] & (1 << (index & 31)));
  }

  void set(size_t index) {
    flags_[index >> 5] |= (1U << (static_cast<uint32>(index) & 31));
  }

  void clear(size_t index) {
    flags_[index >> 5] &= ~(1U << (static_cast<uint32>(index) & 31));
  }

  // Returns the number of flags that can be set/checked.
  size_t size() const { return sizeof(flags_) * 8; }

 private:
  // 256 * 32 = 8192 flags in 1KB.
  uint32 flags_[256];
  DISALLOW_COPY_AND_ASSIGN(FlagArray);
};

class MultiNotifierWorkerFlagArray : public base::PlatformThread::Delegate {
 public:
  MultiNotifierWorkerFlagArray(size_t count, FlagArray* signals,
                               size_t* shared_data, size_t repeats,
                               CrossProcessNotification* notifier)
      : count_(count), signals_(signals), shared_data_(shared_data),
        repeats_(repeats), notifier_(notifier) {
  }
  virtual ~MultiNotifierWorkerFlagArray() {}

  // base::PlatformThread::Delegate:
  virtual void ThreadMain() override {
    for (size_t i = 0; i < repeats_; ++i) {
      notifier_->Wait();
      for (size_t s = 0; s < count_; ++s) {
        if (signals_->is_set(s)) {
          ++shared_data_[s];
          // We don't clear the flag here but simply leave it signaled because
          // we want the other thread to also increment this variable.
        }
      }
      notifier_->Signal();
    }
  }

 private:
  size_t count_;
  FlagArray* signals_;
  size_t* shared_data_;
  size_t repeats_;
  CrossProcessNotification* notifier_;
  DISALLOW_COPY_AND_ASSIGN(MultiNotifierWorkerFlagArray);
};

}  // end namespace

TEST(CrossProcessNotification, FlagArray) {
  FlagArray flags;
  EXPECT_GT(flags.size(), 1000U);
  for (size_t i = 0; i < flags.size(); ++i) {
    EXPECT_FALSE(flags.is_set(i));
    flags.set(i);
    EXPECT_TRUE(flags.is_set(i));
    flags.clear(i);
    EXPECT_FALSE(flags.is_set(i));
  }
}

// Initializes two notifiers, signals the each one and make sure the others
// wait is satisfied.
TEST(CrossProcessNotification, Basic) {
  CrossProcessNotification a, b;
  ASSERT_TRUE(CrossProcessNotification::InitializePair(&a, &b));
  EXPECT_TRUE(a.IsValid());
  EXPECT_TRUE(b.IsValid());

  a.Signal();
  b.Wait();

  b.Signal();
  a.Wait();
}

// Spins two worker threads, each with their own CrossProcessNotification
// that they use to read and write from a shared memory buffer.
// Disabled as it trips of the TSAN bot (false positive since TSAN doesn't
// recognize sockets as being a synchronization primitive).
TEST(CrossProcessNotification, DISABLED_TwoThreads) {
  CrossProcessNotification a, b;
  ASSERT_TRUE(CrossProcessNotification::InitializePair(&a, &b));

  size_t data = 0;
  const size_t kRepeats = 10000;
  SingleNotifierWorker worker1(&data, kRepeats, &a);
  SingleNotifierWorker worker2(&data, kRepeats, &b);
  base::PlatformThreadHandle thread1, thread2;
  base::PlatformThread::Create(0, &worker1, &thread1);
  base::PlatformThread::Create(0, &worker2, &thread2);

  // Start the first thread.  They should ping pong a few times and take turns
  // incrementing the shared variable and never step on each other's toes.
  a.Signal();

  base::PlatformThread::Join(thread1);
  base::PlatformThread::Join(thread2);

  EXPECT_EQ(kRepeats * 2, data);
}

// Uses a pair of threads to access up to 1000 pieces of synchronized shared
// data. On regular dev machines, the number of notifiers should be 1000, but on
// mac and linux bots, the number will be smaller due to the RLIMIT_NOFILE
// limit. Specifically, linux will have this limit at 1024 which means for this
// test that the max number of notifiers will be in the range 500-512. On Mac
// the limit is 256, so |count| will be ~120.  Oh, and raising the limit via
// setrlimit() won't work.
// DISABLED since the distribution won't be accurate when run on valgrind.
TEST(CrossProcessNotification, DISABLED_ThousandNotifiersTwoThreads) {
  const size_t kCount = 1000;
  NotificationsOwner pairs(kCount);
  size_t data[kCount] = {0};
  // We use a multiple of the count so that the division in the check below
  // will be nice and round.
  size_t repeats = pairs.size() * 1;

  MultiNotifierWorker worker_1(&data[0], repeats, &pairs.a());
  MultiNotifierWorker worker_2(&data[0], repeats, &pairs.b());
  base::PlatformThreadHandle thread_1, thread_2;
  base::PlatformThread::Create(0, &worker_1, &thread_1);
  base::PlatformThread::Create(0, &worker_2, &thread_2);

  for (size_t i = 0; i < pairs.size(); ++i)
    pairs.a()[i]->Signal();

  base::PlatformThread::Join(thread_1);
  base::PlatformThread::Join(thread_2);

  size_t expected_total = pairs.size() * 2;
  size_t total = 0;
  for (size_t i = 0; i < pairs.size(); ++i) {
    // The CrossProcessNotification::WaitForMultiple class should have ensured
    // that all notifiers had the same quality of service.
    EXPECT_EQ(expected_total / pairs.size(), data[i]);
    total += data[i];
  }
  EXPECT_EQ(expected_total, total);
}

// Functionally equivalent (as far as the shared data goes) to the
// ThousandNotifiersTwoThreads test but uses a single pair of notifiers +
// FlagArray for the 1000 signals. This approach is significantly faster.
// Disabled as it trips of the TSAN bot - "Possible data race during write of
// size 4" (the flag array).
TEST(CrossProcessNotification, DISABLED_TwoNotifiersTwoThreads1000Signals) {
  CrossProcessNotification a, b;
  ASSERT_TRUE(CrossProcessNotification::InitializePair(&a, &b));

  const size_t kCount = 1000;
  FlagArray signals;
  ASSERT_GE(signals.size(), kCount);
  size_t data[kCount] = {0};

  // Since this algorithm checks all events each time the notifier is
  // signaled, |repeat| doesn't mean the same thing here as it does in
  // ThousandNotifiersTwoThreads.  1 repeat here is the same as kCount
  // repeats in ThousandNotifiersTwoThreads.
  size_t repeats = 1;
  MultiNotifierWorkerFlagArray worker1(kCount, &signals, &data[0], repeats, &a);
  MultiNotifierWorkerFlagArray worker2(kCount, &signals, &data[0], repeats, &b);
  base::PlatformThreadHandle thread1, thread2;
  base::PlatformThread::Create(0, &worker1, &thread1);
  base::PlatformThread::Create(0, &worker2, &thread2);

  for (size_t i = 0; i < kCount; ++i)
    signals.set(i);
  a.Signal();

  base::PlatformThread::Join(thread1);
  base::PlatformThread::Join(thread2);

  size_t expected_total = kCount * 2;
  size_t total = 0;
  for (size_t i = 0; i < kCount; ++i) {
    // Since for each signal, we process all signaled events, the shared data
    // variables should all be equal.
    EXPECT_EQ(expected_total / kCount, data[i]);
    total += data[i];
  }
  EXPECT_EQ(expected_total, total);
}

// Test the maximum number of notifiers without spinning further wait
// threads on Windows. This test assumes we can always create 64 pairs and
// bails if we can't.
TEST(CrossProcessNotification, MultipleWaits64) {
  const size_t kCount = 64;
  NotificationsOwner pairs(kCount);
  ASSERT_TRUE(pairs.size() == kCount);

  CrossProcessNotification::WaitForMultiple waiter(&pairs.b());
  for (size_t i = 0; i < kCount; ++i) {
    pairs.a()[i]->Signal();
    int index = waiter.Wait();
    EXPECT_EQ(i, static_cast<size_t>(index));
  }
}

// Tests waiting for more notifiers than the OS supports on one thread.
// The test will create at most 1000 pairs, but on mac/linux bots the actual
// number will be lower.  See comment about the RLIMIT_NOFILE limit above for
// more details.
// DISABLED since the distribution won't be accurate when run on valgrind.
TEST(CrossProcessNotification, DISABLED_MultipleWaits1000) {
  // A 1000 notifiers requires 16 threads on Windows, including the current
  // one, to perform the wait operation.
  const size_t kCount = 1000;
  NotificationsOwner pairs(kCount);

  for (size_t i = 0; i < pairs.size(); ++i) {
    pairs.a()[i]->Signal();
    // To disable the load distribution algorithm and force the extra worker
    // thread(s) to catch the signaled event, we define the |waiter| inside
    // the loop.
    CrossProcessNotification::WaitForMultiple waiter(&pairs.b());
    int index = waiter.Wait();
    EXPECT_EQ(i, static_cast<size_t>(index));
  }
}

class CrossProcessNotificationMultiProcessTest : public base::MultiProcessTest {
};

namespace {

// A very crude IPC mechanism that we use to set up the spawned child process
// and the parent process.
struct CrudeIpc {
  uint8 ready;
  CrossProcessNotification::IPCHandle handle_1;
  CrossProcessNotification::IPCHandle handle_2;
};

#if defined(OS_POSIX)
const int kPosixChildSharedMem = 30;
#else
const char kSharedMemName[] = "CrossProcessNotificationMultiProcessTest";
#endif

const size_t kSharedMemSize = 1024;

}  // namespace

// The main routine of the child process.  Waits for the parent process
// to copy handles over to the child and then uses a CrossProcessNotification to
// wait and signal to the parent process.
MULTIPROCESS_TEST_MAIN(CrossProcessNotificationChildMain) {
#if defined(OS_POSIX)
  base::SharedMemory mem(
      base::SharedMemoryHandle(kPosixChildSharedMem, true /* auto close */),
      false);
#else
  base::SharedMemory mem;
  CHECK(mem.CreateNamed(kSharedMemName, true, kSharedMemSize));
#endif

  CHECK(mem.Map(kSharedMemSize));
  CrudeIpc* ipc = reinterpret_cast<CrudeIpc*>(mem.memory());

  while (!ipc->ready)
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));

  CrossProcessNotification notifier(ipc->handle_1, ipc->handle_2);
  notifier.Wait();
  notifier.Signal();

  return 0;
}

// Spawns a new process and hands a CrossProcessNotification instance to the
// new process.  Once that's done, it waits for the child process to signal
// it's end and quits.
TEST_F(CrossProcessNotificationMultiProcessTest, Basic) {
  CrossProcessNotification a, b;
  ASSERT_TRUE(CrossProcessNotification::InitializePair(&a, &b));
  EXPECT_TRUE(a.IsValid());
  EXPECT_TRUE(b.IsValid());

  base::SharedMemory mem;

#if defined(OS_POSIX)
  ASSERT_TRUE(mem.CreateAndMapAnonymous(kSharedMemSize));
#else
  mem.Delete(kSharedMemName);  // In case a previous run was unsuccessful.
  ASSERT_TRUE(mem.CreateNamed(kSharedMemName, false, kSharedMemSize));
  ASSERT_TRUE(mem.Map(kSharedMemSize));
#endif

  CrudeIpc* ipc = reinterpret_cast<CrudeIpc*>(mem.memory());
  ipc->ready = false;

#if defined(OS_POSIX)
  const int kPosixChildSocket = 20;
  EXPECT_TRUE(b.ShareToProcess(
        base::kNullProcessHandle, &ipc->handle_1, &ipc->handle_2));
  base::FileHandleMappingVector fd_mapping_vec;
  fd_mapping_vec.push_back(std::make_pair(ipc->handle_1.fd, kPosixChildSocket));
  fd_mapping_vec.push_back(
      std::make_pair(mem.handle().fd, kPosixChildSharedMem));
  ipc->handle_1.fd = kPosixChildSocket;
  base::ProcessHandle process = SpawnChild("CrossProcessNotificationChildMain",
                                           fd_mapping_vec, false);
#else
  base::ProcessHandle process = SpawnChild("CrossProcessNotificationChildMain",
                                           false);
  EXPECT_TRUE(b.ShareToProcess(process, &ipc->handle_1, &ipc->handle_2));
#endif

  ipc->ready = true;

  a.Signal();
  a.Wait();

  int exit_code = -1;
  base::WaitForExitCode(process, &exit_code);
  EXPECT_EQ(0, exit_code);
}
