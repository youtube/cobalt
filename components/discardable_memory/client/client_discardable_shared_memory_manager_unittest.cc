// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/memory/page_size.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/discardable_memory/public/mojom/discardable_shared_memory_manager.mojom.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_COBALT)
#include "base/features.h"
#include "base/test/scoped_feature_list.h"
#endif  // BUILDFLAG(IS_COBALT)

namespace discardable_memory {
namespace {

using base::Location;
using base::OnceClosure;

class TestSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
  ~TestSingleThreadTaskRunner() override = default;
  bool PostTask(const Location& from_here, OnceClosure task) { return true; }
  template <class T>
  bool DeleteSoon(const Location& from_here, const T* object) {
    return true;
  }
  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       base::TimeDelta delay) override {
    return true;
  }
  bool PostNonNestableDelayedTask(const Location& from_here,
                                  OnceClosure task,
                                  base::TimeDelta delay) override {
    return true;
  }
  bool RunsTasksInCurrentSequence() const override { return true; }
};

class TestClientDiscardableSharedMemoryManager
    : public ClientDiscardableSharedMemoryManager {
 public:
  TestClientDiscardableSharedMemoryManager()
      : ClientDiscardableSharedMemoryManager(
            base::MakeRefCounted<TestSingleThreadTaskRunner>()) {}

  std::unique_ptr<base::DiscardableSharedMemory>
  AllocateLockedDiscardableSharedMemory(size_t size, int32_t id) override {
    auto shared_memory = std::make_unique<base::DiscardableSharedMemory>();
    shared_memory->CreateAndMap(size);
    return shared_memory;
  }

  void DeletedDiscardableSharedMemory(int32_t id) override {}

  size_t GetSize() const {
    base::AutoLock lock(lock_);
    return heap_->GetSize();
  }

  size_t GetFreelistSize() const {
    base::AutoLock lock(lock_);
    return heap_->GetFreelistSize();
  }

  bool IsPurgeScheduled() const {
    base::AutoLock lock(lock_);
    return is_purge_scheduled_;
  }

 private:
  ~TestClientDiscardableSharedMemoryManager() override = default;
};

class ClientDiscardableSharedMemoryManagerTest : public testing::Test {
 public:
  ClientDiscardableSharedMemoryManagerTest()
      : task_env_(base::test::TaskEnvironment::MainThreadType::UI,
                  base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  const size_t page_size_ = base::GetPageSize();
  base::test::TaskEnvironment task_env_;
};

// This test allocates a single piece of memory, then verifies that calling
// |BackgroundPurge| only affects the memory when it is unlocked.
TEST_F(ClientDiscardableSharedMemoryManagerTest, Simple) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  // Initially, we should have no memory allocated
  ASSERT_EQ(client->GetBytesAllocated(), 0u);
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem = client->AllocateLockedDiscardableMemory(page_size_);

  // After allocation, we should have allocated a single piece of memory.
  EXPECT_EQ(client->GetBytesAllocated(), page_size_);

  client->BackgroundPurge();

  // All our memory is locked, so calling |BackgroundPurge| should have no
  // effect.
  EXPECT_EQ(client->GetBytesAllocated(), base::GetPageSize());

  mem->Unlock();

  // Unlocking has no effect on the amount of memory we have allocated.
  EXPECT_EQ(client->GetBytesAllocated(), base::GetPageSize());

  client->BackgroundPurge();

  // Now that |mem| is unlocked, the call to |BackgroundPurge| will
  // remove it.
  EXPECT_EQ(client->GetBytesAllocated(), 0u);
}

// This test allocates multiple pieces of memory, then unlocks them one by one,
// verifying that |BackgroundPurge| only affects the unlocked pieces of
// memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest, MultipleOneByOne) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetBytesAllocated(), 0u);
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 2.2);
  auto mem2 = client->AllocateLockedDiscardableMemory(page_size_ * 1.1);
  auto mem3 = client->AllocateLockedDiscardableMemory(page_size_ * 3.5);
  auto mem4 = client->AllocateLockedDiscardableMemory(page_size_ * 0.2);

  EXPECT_EQ(client->GetBytesAllocated(), 10 * page_size_);

  // Does nothing because everything is locked.
  client->BackgroundPurge();

  EXPECT_EQ(client->GetBytesAllocated(), 10 * page_size_);

  mem1->Unlock();

  // Does nothing, since we don't have any free memory, just unlocked memory.
  client->ReleaseFreeMemory();

  EXPECT_EQ(client->GetBytesAllocated(), 10 * page_size_);

  // This gets rid of |mem1| (which is unlocked), but not the rest of the
  // memory.
  client->BackgroundPurge();

  EXPECT_EQ(client->GetBytesAllocated(), 7 * page_size_);

  // We do similar checks to above for the rest of the memory.
  mem2->Unlock();

  client->BackgroundPurge();

  EXPECT_EQ(client->GetBytesAllocated(), 5 * page_size_);

  mem3->Unlock();

  client->BackgroundPurge();
  EXPECT_EQ(client->GetBytesAllocated(), 1 * page_size_);

  mem4->Unlock();

  client->BackgroundPurge();
  EXPECT_EQ(client->GetBytesAllocated(), 0 * page_size_);
}

// This test allocates multiple pieces of memory, then unlocks them all,
// verifying that |BackgroundPurge| only affects the unlocked pieces of
// memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest, MultipleAtOnce) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetBytesAllocated(), 0u);
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 2.2);
  auto mem2 = client->AllocateLockedDiscardableMemory(page_size_ * 1.1);
  auto mem3 = client->AllocateLockedDiscardableMemory(page_size_ * 3.5);
  auto mem4 = client->AllocateLockedDiscardableMemory(page_size_ * 0.2);

  EXPECT_EQ(client->GetBytesAllocated(), 10 * page_size_);

  // Does nothing because everything is locked.
  client->BackgroundPurge();

  EXPECT_EQ(client->GetBytesAllocated(), 10 * page_size_);

  // Unlock all pieces of memory at once.
  mem1->Unlock();
  mem2->Unlock();
  mem3->Unlock();
  mem4->Unlock();

  client->BackgroundPurge();
  EXPECT_EQ(client->GetBytesAllocated(), 0 * page_size_);
}

// Tests that FreeLists are only released once all memory has been released.
TEST_F(ClientDiscardableSharedMemoryManagerTest, Release) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetBytesAllocated(), 0u);
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 3);
  auto mem2 = client->AllocateLockedDiscardableMemory(page_size_ * 2);

  size_t freelist_size = client->GetFreelistSize();
  EXPECT_EQ(client->GetBytesAllocated(), 5 * page_size_);

  mem1 = nullptr;

  // Less memory is now allocated, but freelists are grown.
  EXPECT_EQ(client->GetBytesAllocated(), page_size_ * 2);
  EXPECT_EQ(client->GetFreelistSize(), freelist_size + page_size_ * 3);

  client->BackgroundPurge();

  // Purging doesn't remove any memory since none is unlocked, also doesn't
  // remove freelists since we still have some.
  EXPECT_EQ(client->GetBytesAllocated(), page_size_ * 2);
  EXPECT_EQ(client->GetFreelistSize(), freelist_size + page_size_ * 3);

  mem2 = nullptr;

  // No memory is allocated, but freelists are grown.
  EXPECT_EQ(client->GetBytesAllocated(), 0u);
  EXPECT_EQ(client->GetFreelistSize(), freelist_size + page_size_ * 5);

  client->BackgroundPurge();

  // Purging now shrinks freelists as well.
  EXPECT_EQ(client->GetBytesAllocated(), 0u);
  EXPECT_EQ(client->GetFreelistSize(), 0u);
}

// Similar to previous test, but makes sure that freelist still shrinks when
// last piece of memory was just unlocked instead of released.
TEST_F(ClientDiscardableSharedMemoryManagerTest, ReleaseUnlocked) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetBytesAllocated(), 0u);
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 3);
  auto mem2 = client->AllocateLockedDiscardableMemory(page_size_ * 2);

  size_t freelist_size = client->GetFreelistSize();
  EXPECT_EQ(client->GetBytesAllocated(), 5 * page_size_);

  mem1 = nullptr;

  // Less memory is now allocated, but freelists are grown.
  EXPECT_EQ(client->GetBytesAllocated(), page_size_ * 2);
  EXPECT_EQ(client->GetFreelistSize(), freelist_size + page_size_ * 3);

  client->BackgroundPurge();

  // Purging doesn't remove any memory since none is unlocked, also doesn't
  // remove freelists since we still have some.
  EXPECT_EQ(client->GetBytesAllocated(), page_size_ * 2);
  EXPECT_EQ(client->GetFreelistSize(), freelist_size + page_size_ * 3);

  mem2->Unlock();

  // No change in memory usage, since memory was only unlocked not released.
  EXPECT_EQ(client->GetBytesAllocated(), page_size_ * 2);
  EXPECT_EQ(client->GetFreelistSize(), freelist_size + page_size_ * 3);

  client->BackgroundPurge();

  // Purging now shrinks freelists as well.
  EXPECT_EQ(client->GetBytesAllocated(), 0u);
  EXPECT_EQ(client->GetFreelistSize(), 0u);
}

// This tests that memory is actually removed by the periodic purging. We mock a
// task runner for this test and fast forward to make sure that the memory is
// purged at the right time.
TEST_F(ClientDiscardableSharedMemoryManagerTest, ScheduledReleaseUnlocked) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetBytesAllocated(), 0u);
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 3);

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client->GetBytesAllocated(), 3 * page_size_);

  mem1->Unlock();

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client->GetBytesAllocated(), 0u);
}

// Same as the above test, but tests that multiple pieces of memory will be
// handled properly.
TEST_F(ClientDiscardableSharedMemoryManagerTest,
       ScheduledReleaseUnlockedMultiple) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_EQ(client->GetBytesAllocated(), 0u);
  ASSERT_EQ(client->GetFreelistSize(), 0u);

  auto mem1 = client->AllocateLockedDiscardableMemory(page_size_ * 3);
  auto mem2 = client->AllocateLockedDiscardableMemory(page_size_ * 2);

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client->GetBytesAllocated(), 5 * page_size_);

  mem1->Unlock();

  EXPECT_EQ(client->GetBytesAllocated(), 5 * page_size_);

  // The purge only removes things that have been unlocked for at least
  // |kMinAgeForScheduledPurge|
  // minutes so this shouldn't remove anything.
  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge / 2);

  EXPECT_EQ(client->GetBytesAllocated(), 5 * page_size_);

  // The periodic purge should remove anything that's been locked for over
  // |kMinAgeForScheduledPurge|
  // minutes, so fast forward slightly more so that it happens.
  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge / 2);

  EXPECT_EQ(client->GetBytesAllocated(), 2 * page_size_);

  mem2->Unlock();

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kMinAgeForScheduledPurge * 2);

  EXPECT_EQ(client->GetBytesAllocated(), 0u);
  EXPECT_EQ(client->GetFreelistSize(), 0u);
}

// Tests that the UMA for Lock()-ing successes
// ("Memory.Discardable.LockingSuccess") is recorded properly.
TEST_F(ClientDiscardableSharedMemoryManagerTest, LockingSuccessUma) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  base::HistogramTester histograms;

  // number is arbitrary, we are only focused on whether the histogram is
  // properly recorded in this test.
  auto mem = client->AllocateLockedDiscardableMemory(200);

  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", false, 0);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", true, 0);

  // Unlock then lock. This should add one sample to the success bucket.
  mem->Unlock();
  bool result = mem->Lock();

  ASSERT_EQ(result, true);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", false, 0);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", true, 1);

  // Repeat the above to verify a second sample is added.
  mem->Unlock();
  result = mem->Lock();

  ASSERT_EQ(result, true);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", false, 0);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", true, 2);

  // This should now fail because the unlocked memory was purged. This should
  // add a sample to the failure bucket.
  mem->Unlock();
  client->BackgroundPurge();
  result = mem->Lock();

  ASSERT_EQ(result, false);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", false, 1);
  histograms.ExpectBucketCount("Memory.Discardable.LockingSuccess", true, 2);
}

// Test that a repeating timer for background purging is created when we
// allocate memory and discarded when we run out of allocated memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest, SchedulingProactivePurging) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_FALSE(client->IsPurgeScheduled());

  // the amount of memory allocated here is arbitrary, we're only trying to get
  // the timer started.
  auto mem = client->AllocateLockedDiscardableMemory(200);
  EXPECT_TRUE(client->IsPurgeScheduled());

  // This does not stop periodic purge because there is still memory allocated.
  client->ReleaseFreeMemory();
  EXPECT_TRUE(client->IsPurgeScheduled());

  // Even though no memory is allocated, periodic purge will stop running after
  // the next scheduled task.
  mem = nullptr;
  EXPECT_TRUE(client->IsPurgeScheduled());
  client->ReleaseFreeMemory();
  EXPECT_TRUE(client->IsPurgeScheduled());

  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kScheduledPurgeInterval);
  EXPECT_FALSE(client->IsPurgeScheduled());
}

// This test is similar to the one above, but tests that creating and deleting
// the timer still works with multiple pieces of allocated memory.
TEST_F(ClientDiscardableSharedMemoryManagerTest,
       SchedulingProactivePurgingMultipleAllocations) {
  auto client =
      base::MakeRefCounted<TestClientDiscardableSharedMemoryManager>();
  ASSERT_FALSE(client->IsPurgeScheduled());

  // the amount of memory allocated here is arbitrary, we're only trying to get
  // the timer started.
  auto mem = client->AllocateLockedDiscardableMemory(200);
  auto mem2 = client->AllocateLockedDiscardableMemory(100);

  task_env_.FastForwardBy(base::Seconds(0));
  EXPECT_TRUE(client->IsPurgeScheduled());

  client->ReleaseFreeMemory();
  EXPECT_TRUE(client->IsPurgeScheduled());

  mem = nullptr;
  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kScheduledPurgeInterval);
  EXPECT_TRUE(client->IsPurgeScheduled());

  client->ReleaseFreeMemory();
  EXPECT_TRUE(client->IsPurgeScheduled());

  mem2 = nullptr;
  task_env_.FastForwardBy(
      ClientDiscardableSharedMemoryManager::kScheduledPurgeInterval);
  EXPECT_FALSE(client->IsPurgeScheduled());
}

// --- Real Mojo IPC tests for Map() failure deleting the service segment ---

class TestDiscardableSharedMemoryManager
    : public DiscardableSharedMemoryManager {
 public:
  size_t ClientSegmentCount(int client_id) const {
    base::AutoLock lock(lock_);
    auto it = clients_.find(client_id);
    return it == clients_.end() ? 0u : it->second.size();
  }

  bool HasClientSegment(int client_id, int32_t id) const {
    base::AutoLock lock(lock_);
    auto it = clients_.find(client_id);
    if (it == clients_.end()) {
      return false;
    }
    return it->second.find(id) != it->second.end();
  }

  std::vector<int32_t> GetClientSegmentIds(int client_id) const {
    base::AutoLock lock(lock_);
    std::vector<int32_t> ids;
    auto it = clients_.find(client_id);
    if (it == clients_.end()) {
      return ids;
    }
    for (const auto& entry : it->second) {
      ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
  }

  int last_client_id() const { return next_client_id_ - 1; }
};

class MapHookClientDiscardableSharedMemoryManager
    : public ClientDiscardableSharedMemoryManager {
 public:
  MapHookClientDiscardableSharedMemoryManager(
      mojo::PendingRemote<mojom::DiscardableSharedMemoryManager> manager,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : ClientDiscardableSharedMemoryManager(std::move(manager),
                                             std::move(io_task_runner)) {}

  void set_fail_map(bool fail) { fail_map_ = fail; }
  void set_on_map_failed(base::RepeatingClosure callback) {
    on_map_failed_ = std::move(callback);
  }

  size_t GetSize() const {
    base::AutoLock lock(lock_);
    return heap_->GetSize();
  }

  void FlushMojo() { FlushMojoForTesting(); }

 protected:
  bool MapDiscardableSharedMemory(base::DiscardableSharedMemory* memory,
                                  size_t size) override {
    if (fail_map_) {
      if (on_map_failed_) {
        on_map_failed_.Run();
      }
      return false;
    }
    return ClientDiscardableSharedMemoryManager::MapDiscardableSharedMemory(
        memory, size);
  }

 private:
  ~MapHookClientDiscardableSharedMemoryManager() override = default;

  bool fail_map_ = false;
  base::RepeatingClosure on_map_failed_;
};

#if BUILDFLAG(IS_COBALT)
// Sets features before a fixture starts any thread, and restores them after
// the fixture has stopped its threads.
class ScopedTestFeatures {
 public:
  ScopedTestFeatures(const std::vector<base::test::FeatureRef>& enabled,
                     const std::vector<base::test::FeatureRef>& disabled) {
    if (!enabled.empty() || !disabled.empty()) {
      features_.InitWithFeatures(enabled, disabled);
    }
  }
  ScopedTestFeatures(const ScopedTestFeatures&) = delete;
  ScopedTestFeatures& operator=(const ScopedTestFeatures&) = delete;

 private:
  base::test::ScopedFeatureList features_;
};
#endif  // BUILDFLAG(IS_COBALT)

class ClientDiscardableSharedMemoryManagerIpcTest : public testing::Test {
 protected:
  ClientDiscardableSharedMemoryManagerIpcTest() = default;
#if BUILDFLAG(IS_COBALT)
  explicit ClientDiscardableSharedMemoryManagerIpcTest(
      const std::vector<base::test::FeatureRef>& enabled)
      : features_(enabled, {}) {}
#endif  // BUILDFLAG(IS_COBALT)

  void SetUp() override {
    base::Thread::Options io_options(base::MessagePumpType::IO, 0);
    ASSERT_TRUE(io_thread_.StartWithOptions(std::move(io_options)));
    service_ = std::make_unique<TestDiscardableSharedMemoryManager>();

    mojo::PendingRemote<mojom::DiscardableSharedMemoryManager> remote;
    auto receiver = remote.InitWithNewPipeAndPassReceiver();
    base::WaitableEvent bound(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](DiscardableSharedMemoryManager* mgr,
               mojo::PendingReceiver<mojom::DiscardableSharedMemoryManager> rx,
               base::WaitableEvent* ev) {
              mgr->Bind(std::move(rx));
              ev->Signal();
            },
            base::Unretained(service_.get()), std::move(receiver), &bound));
    bound.Wait();
    client_id_ = service_->last_client_id();

    client_ = base::MakeRefCounted<MapHookClientDiscardableSharedMemoryManager>(
        std::move(remote), io_thread_.task_runner());
  }

  void TearDown() override {
    client_.reset();
    // The client's ScheduledPurge task holds the last reference. Run it while
    // the IO thread and the service are alive, so that ~Client... posts
    // DeleteSoon(manager_mojo_) to a running IO thread.
    task_env_.FastForwardBy(
        ClientDiscardableSharedMemoryManager::kScheduledPurgeInterval);
    // Run that DeleteSoon() and drain the Mojo disconnect so that
    // SelfOwnedReceiver is destroyed before ~service_.
    io_thread_.FlushForTesting();
    base::WaitableEvent drained(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    io_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WaitableEvent* ev) {
                         base::RunLoop().RunUntilIdle();
                         ev->Signal();
                       },
                       &drained));
    drained.Wait();
    service_.reset();
    io_thread_.Stop();
  }

  const size_t page_size_ = base::GetPageSize();
#if BUILDFLAG(IS_COBALT)
  // Before anything that starts a thread.
  ScopedTestFeatures features_{{}, {}};
#endif  // BUILDFLAG(IS_COBALT)
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::Thread io_thread_{"discardable_test_io"};
  std::unique_ptr<TestDiscardableSharedMemoryManager> service_;
  scoped_refptr<MapHookClientDiscardableSharedMemoryManager> client_;
  int client_id_ = -1;
};

void VerifyMapFailureDeletesServiceSegmentWithBaseline(
    TestDiscardableSharedMemoryManager* service,
    MapHookClientDiscardableSharedMemoryManager* client,
    int client_id,
    size_t page_size) {
  // Allocate a baseline segment and keep it live (filling the whole chunk so
  // the next allocation must request a new segment from the service).
  auto first = client->AllocateLockedDiscardableMemory(page_size);
  ASSERT_TRUE(first);
  const size_t chunk_size = client->GetSize();
  auto filler =
      client->AllocateLockedDiscardableMemory(chunk_size - page_size);
  ASSERT_TRUE(filler);

  const size_t baseline_bytes = service->GetBytesAllocated();
  ASSERT_GT(baseline_bytes, 0u);
  const std::vector<int32_t> baseline_ids =
      service->GetClientSegmentIds(client_id);
  ASSERT_EQ(baseline_ids.size(), 1u);
  const int32_t baseline_id = baseline_ids[0];

  // Trigger a second segment allocation whose service-side creation and dup()
  // succeed, but client-side Map() fails.
  std::optional<int32_t> failed_id;
  size_t bytes_during_map = 0;
  size_t segments_during_map = 0;
  client->set_fail_map(true);
  client->set_on_map_failed(base::BindRepeating(
      [](TestDiscardableSharedMemoryManager* svc, int cid, int32_t base_id,
         std::optional<int32_t>* out_id, size_t* out_bytes, size_t* out_count) {
        *out_bytes = svc->GetBytesAllocated();
        auto ids = svc->GetClientSegmentIds(cid);
        *out_count = ids.size();
        for (int32_t id : ids) {
          if (id != base_id) {
            *out_id = id;
          }
        }
      },
      base::Unretained(service), client_id, baseline_id, &failed_id,
      &bytes_during_map, &segments_during_map));

  auto failed_alloc = client->AllocateLockedDiscardableMemory(page_size);
  EXPECT_EQ(failed_alloc, nullptr);

  // Confirm the service actually allocated and returned a segment before Map()
  // failed.
  EXPECT_EQ(segments_during_map, 2u);
  EXPECT_GT(bytes_during_map, baseline_bytes);
  ASSERT_TRUE(failed_id.has_value());
  EXPECT_NE(*failed_id, baseline_id);

  // Flush the client's Mojo pipe while keeping |client| alive and connected (so
  // ClientRemoved() cannot fire and mask a leak).
  client->FlushMojo();

  EXPECT_EQ(service->GetBytesAllocated(), baseline_bytes);
  EXPECT_EQ(service->ClientSegmentCount(client_id), 1u);
  EXPECT_TRUE(service->HasClientSegment(client_id, baseline_id));
  EXPECT_FALSE(service->HasClientSegment(client_id, *failed_id));

  // Verify the service no longer reserves |failed_id| by allocating it again.
  base::UnsafeSharedMemoryRegion probe_region;
  service->AllocateLockedDiscardableSharedMemoryForClient(
      client_id, page_size, *failed_id, &probe_region);
  EXPECT_TRUE(probe_region.IsValid());
  service->ClientDeletedDiscardableSharedMemory(*failed_id, client_id);
  EXPECT_EQ(service->GetBytesAllocated(), baseline_bytes);
  EXPECT_FALSE(service->HasClientSegment(client_id, *failed_id));

  // Normal allocations continue to succeed after re-enabling Map().
  client->set_fail_map(false);
  auto after = client->AllocateLockedDiscardableMemory(page_size);
  EXPECT_TRUE(after);
  EXPECT_EQ(service->ClientSegmentCount(client_id), 2u);
}

TEST_F(ClientDiscardableSharedMemoryManagerIpcTest,
       MapFailureDeletesServiceSegmentWithBaseline) {
  VerifyMapFailureDeletesServiceSegmentWithBaseline(
      service_.get(), client_.get(), client_id_, page_size_);
}

#if BUILDFLAG(IS_COBALT)
class ClientDiscardableSharedMemoryManagerLazyMappingIpcTest
    : public ClientDiscardableSharedMemoryManagerIpcTest {
 protected:
  ClientDiscardableSharedMemoryManagerLazyMappingIpcTest()
      : ClientDiscardableSharedMemoryManagerIpcTest(
            {base::features::kCobaltDiscardableSingleMapping}) {}
};

TEST_F(ClientDiscardableSharedMemoryManagerLazyMappingIpcTest,
       MapFailureDeletesServiceSegmentWithBaseline) {
  VerifyMapFailureDeletesServiceSegmentWithBaseline(
      service_.get(), client_.get(), client_id_, page_size_);
}
#endif  // BUILDFLAG(IS_COBALT)

TEST_F(ClientDiscardableSharedMemoryManagerIpcTest,
       MapFailureDeletesServiceSegmentFromZero) {
  ASSERT_EQ(service_->GetBytesAllocated(), 0u);
  ASSERT_EQ(service_->ClientSegmentCount(client_id_), 0u);

  std::optional<int32_t> failed_id;
  size_t bytes_during_map = 0;
  client_->set_fail_map(true);
  client_->set_on_map_failed(base::BindRepeating(
      [](TestDiscardableSharedMemoryManager* svc, int cid,
         std::optional<int32_t>* out_id, size_t* out_bytes) {
        *out_bytes = svc->GetBytesAllocated();
        auto ids = svc->GetClientSegmentIds(cid);
        if (ids.size() == 1u) {
          *out_id = ids[0];
        }
      },
      base::Unretained(service_.get()), client_id_, &failed_id,
      &bytes_during_map));

  auto failed_alloc = client_->AllocateLockedDiscardableMemory(page_size_);
  EXPECT_EQ(failed_alloc, nullptr);
  EXPECT_GT(bytes_during_map, 0u);
  ASSERT_TRUE(failed_id.has_value());

  client_->FlushMojo();

  EXPECT_EQ(service_->GetBytesAllocated(), 0u);
  EXPECT_EQ(service_->ClientSegmentCount(client_id_), 0u);
  EXPECT_FALSE(service_->HasClientSegment(client_id_, *failed_id));

  client_->set_fail_map(false);
  auto good = client_->AllocateLockedDiscardableMemory(page_size_);
  EXPECT_TRUE(good);
  EXPECT_EQ(service_->ClientSegmentCount(client_id_), 1u);
}

}  // namespace
}  // namespace discardable_memory
