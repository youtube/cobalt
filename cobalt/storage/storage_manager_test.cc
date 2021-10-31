// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/storage/storage_manager.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/storage/savegame_fake.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::Eq;

namespace cobalt {
namespace storage {

namespace {

// Used to be able to intercept QueueFlush().
class MockStorageManager : public StorageManager {
 public:
  MockStorageManager(
      std::unique_ptr<StorageManager::UpgradeHandler> upgrade_handler,
      const Options& options)
      : StorageManager(std::move(upgrade_handler), options) {}
#ifndef GMOCK_NO_MOVE_MOCK
  MOCK_METHOD1(QueueFlush, void(base::OnceClosure callback));
#endif
};

class MockUpgradeHandler : public StorageManager::UpgradeHandler {
 public:
  MOCK_METHOD3(OnUpgrade, void(StorageManager*, const char*, int));
};

class CallbackWaiter {
 public:
  CallbackWaiter()
      : was_called_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  virtual ~CallbackWaiter() {}
  bool TimedWait() {
    return was_called_event_.TimedWait(base::TimeDelta::FromSeconds(5));
  }
  bool IsSignaled() { return was_called_event_.IsSignaled(); }

 protected:
  void Signal() { was_called_event_.Signal(); }

 private:
  base::WaitableEvent was_called_event_;

  DISALLOW_COPY_AND_ASSIGN(CallbackWaiter);
};

class FlushWaiter : public CallbackWaiter {
 public:
  FlushWaiter() {}
  void OnFlushDone() { Signal(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(FlushWaiter);
};

class MemoryStoreWaiter : public CallbackWaiter {
 public:
  MemoryStoreWaiter() {}
  void OnMemoryStore(MemoryStore* memory_store) {
    Signal();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryStoreWaiter);
};

class ReadOnlyMemoryStoreWaiter : public CallbackWaiter {
 public:
  ReadOnlyMemoryStoreWaiter() {}
  void OnMemoryStore(const MemoryStore& memory_store) {
    Signal();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadOnlyMemoryStoreWaiter);
};

void FlushCallback(MemoryStore* memory_store) {
  EXPECT_NE(memory_store, nullptr);
}

}  // namespace

class StorageManagerTest : public ::testing::Test {
 protected:
  StorageManagerTest() : message_loop_(base::MessageLoop::TYPE_DEFAULT) {}

  ~StorageManagerTest() { storage_manager_.reset(NULL); }

  template <typename StorageManagerType>
  void Init(bool delete_savegame = true,
            const Savegame::ByteVector* initial_data = NULL) {
    // Destroy the current one first. We can't have two VFSs with the same name
    // concurrently.
    storage_manager_.reset(NULL);

    std::unique_ptr<StorageManager::UpgradeHandler> upgrade_handler(
        new MockUpgradeHandler());
    StorageManager::Options options;
    options.savegame_options.delete_on_destruction = delete_savegame;
    options.savegame_options.factory = &SavegameFake::Create;
    if (initial_data) {
      options.savegame_options.test_initial_data = *initial_data;
    }
    storage_manager_.reset(
        new StorageManagerType(std::move(upgrade_handler), options));
  }

  template <typename StorageManagerType>
  void FinishIO() {
    storage_manager_->FinishIO();
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<StorageManager> storage_manager_;
};

TEST_F(StorageManagerTest, WithMemoryStore) {
  Init<StorageManager>();
  MemoryStoreWaiter waiter;
  storage_manager_->WithMemoryStore(
      base::Bind(&MemoryStoreWaiter::OnMemoryStore, base::Unretained(&waiter)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(waiter.TimedWait());
}

TEST_F(StorageManagerTest, WithReadOnlyMemoryStore) {
  Init<StorageManager>();
  ReadOnlyMemoryStoreWaiter waiter;
  storage_manager_->WithReadOnlyMemoryStore(base::Bind(
      &ReadOnlyMemoryStoreWaiter::OnMemoryStore, base::Unretained(&waiter)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(waiter.TimedWait());
}

TEST_F(StorageManagerTest, FlushNow) {
  // Ensure the Flush callback is called.
  Init<StorageManager>();
  storage_manager_->WithMemoryStore(base::Bind(&FlushCallback));
  base::RunLoop().RunUntilIdle();
  FlushWaiter waiter;
  storage_manager_->FlushNow(
      base::Bind(&FlushWaiter::OnFlushDone, base::Unretained(&waiter)));
  EXPECT_TRUE(waiter.TimedWait());
}

#ifndef GMOCK_NO_MOVE_MOCK
TEST_F(StorageManagerTest, FlushNowWithFlushOnChange) {
  // Test that the Flush callback is called exactly once, despite calling both
  // FlushOnChange() and FlushNow().
  Init<MockStorageManager>();

  storage_manager_->WithMemoryStore(base::Bind(&FlushCallback));
  base::RunLoop().RunUntilIdle();

  FlushWaiter waiter;
  MockStorageManager& storage_manager =
      *dynamic_cast<MockStorageManager*>(storage_manager_.get());

  // When QueueFlush() is called, have it also call FlushWaiter::OnFlushDone().
  ON_CALL(storage_manager, QueueFlush(_))
      .WillByDefault(InvokeWithoutArgs(&waiter, &FlushWaiter::OnFlushDone));
  EXPECT_CALL(storage_manager, QueueFlush(_)).Times(1);

  storage_manager_->FlushOnChange();
  storage_manager_->FlushNow(
      base::Bind(&FlushWaiter::OnFlushDone, base::Unretained(&waiter)));

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(3000));

  EXPECT_TRUE(waiter.IsSignaled());
}

TEST_F(StorageManagerTest, FlushOnChange) {
  // Test that the Flush callback is called exactly once, despite calling
  // FlushOnChange() multiple times.
  Init<MockStorageManager>();

  storage_manager_->WithMemoryStore(base::Bind(&FlushCallback));
  base::RunLoop().RunUntilIdle();

  FlushWaiter waiter;
  MockStorageManager& storage_manager =
      *dynamic_cast<MockStorageManager*>(storage_manager_.get());

  // When QueueFlush() is called, have it also call FlushWaiter::OnFlushDone().
  // We will wait for this in TimedWait().
  ON_CALL(storage_manager, QueueFlush(_))
      .WillByDefault(InvokeWithoutArgs(&waiter, &FlushWaiter::OnFlushDone));
  EXPECT_CALL(storage_manager, QueueFlush(_)).Times(1);

  for (int i = 0; i < 10; ++i) {
    storage_manager_->FlushOnChange();
  }

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(3000));

  EXPECT_TRUE(waiter.TimedWait());
}

TEST_F(StorageManagerTest, FlushOnChangeMaxDelay) {
  // Test that the Flush callback is called once from hitting the max delay when
  // there are constant calls to FlushOnChange().
  Init<MockStorageManager>();

  storage_manager_->WithMemoryStore(base::Bind(&FlushCallback));
  base::RunLoop().RunUntilIdle();

  FlushWaiter waiter;
  MockStorageManager& storage_manager =
      *dynamic_cast<MockStorageManager*>(storage_manager_.get());

  // When QueueFlush() is called, have it also call FlushWaiter::OnFlushDone().
  ON_CALL(storage_manager, QueueFlush(_))
      .WillByDefault(InvokeWithoutArgs(&waiter, &FlushWaiter::OnFlushDone));
  EXPECT_CALL(storage_manager, QueueFlush(_)).Times(1);

  for (int i = 0; i < 30; ++i) {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
    storage_manager_->FlushOnChange();
  }

  EXPECT_TRUE(waiter.IsSignaled());
}

TEST_F(StorageManagerTest, FlushOnShutdown) {
  // Test that pending flushes are completed on shutdown.
  Init<MockStorageManager>();

  storage_manager_->WithMemoryStore(base::Bind(&FlushCallback));
  base::RunLoop().RunUntilIdle();

  FlushWaiter waiter;
  MockStorageManager& storage_manager =
      *dynamic_cast<MockStorageManager*>(storage_manager_.get());

  // When QueueFlush() is called, have it also call FlushWaiter::OnFlushDone().
  ON_CALL(storage_manager, QueueFlush(_))
      .WillByDefault(InvokeWithoutArgs(&waiter, &FlushWaiter::OnFlushDone));
  EXPECT_CALL(storage_manager, QueueFlush(_)).Times(1);

  storage_manager_->FlushOnChange();
  FinishIO<StorageManager>();
  storage_manager_.reset();

  EXPECT_TRUE(waiter.IsSignaled());
}
#endif

TEST_F(StorageManagerTest, Upgrade) {
  Savegame::ByteVector initial_data;
  initial_data.push_back('U');
  initial_data.push_back('P');
  initial_data.push_back('G');
  initial_data.push_back('0');
  Init<StorageManager>(true, &initial_data);

  // We expect a call to the upgrade handler when it reads this data.
  MockUpgradeHandler& upgrade_handler =
      *dynamic_cast<MockUpgradeHandler*>(storage_manager_->upgrade_handler());
  EXPECT_CALL(upgrade_handler,
              OnUpgrade(Eq(storage_manager_.get()), NotNull(), Eq(4)))
      .Times(1);

  FlushWaiter waiter;
  storage_manager_->FlushNow(
      base::Bind(&FlushWaiter::OnFlushDone, base::Unretained(&waiter)));
  EXPECT_TRUE(waiter.TimedWait());
  base::RunLoop().RunUntilIdle();
}

}  // namespace storage
}  // namespace cobalt
