// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
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
  MockStorageManager(scoped_ptr<StorageManager::UpgradeHandler> upgrade_handler,
                     const Options& options)
      : StorageManager(upgrade_handler.Pass(), options) {}
  MOCK_METHOD1(QueueFlush, void(const base::Closure& callback));
};

class MockUpgradeHandler : public StorageManager::UpgradeHandler {
 public:
  MOCK_METHOD3(OnUpgrade, void(StorageManager*, const char*, int));
};

class CallbackWaiter {
 public:
  CallbackWaiter() : was_called_event_(true, false) {}
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

class SqlWaiter : public CallbackWaiter {
 public:
  SqlWaiter() {}
  void OnSqlConnection(SqlContext* sql_context) {
    UNREFERENCED_PARAMETER(sql_context);
    Signal();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SqlWaiter);
};

void FlushCallback(SqlContext* sql_context) {
  sql::Connection* conn = sql_context->sql_connection();
  bool ok = conn->Execute("CREATE TABLE FlushTest(test_name TEXT);");
  EXPECT_TRUE(ok);
}

void QuerySchemaCallback(SqlContext* sql_context) {
  int schema_version;
  EXPECT_FALSE(
      sql_context->GetSchemaVersion("Nonexistent table", &schema_version));

  sql::Connection* conn = sql_context->sql_connection();
  bool ok = conn->Execute("CREATE TABLE TestTable(test_name TEXT);");
  EXPECT_TRUE(ok);

  EXPECT_TRUE(sql_context->GetSchemaVersion("TestTable", &schema_version));
  EXPECT_EQ(static_cast<int>(StorageManager::kSchemaTableIsNew),
            schema_version);

  sql_context->UpdateSchemaVersion("TestTable", 100);
}

void InspectSchemaVersionCallback(SqlContext* sql_context) {
  int schema_version;
  EXPECT_TRUE(sql_context->GetSchemaVersion("TestTable", &schema_version));
  EXPECT_EQ(100, schema_version);
}

}  // namespace

class StorageManagerTest : public ::testing::Test {
 protected:
  StorageManagerTest() : message_loop_(MessageLoop::TYPE_DEFAULT) {}

  ~StorageManagerTest() { storage_manager_.reset(NULL); }

  template <typename StorageManagerType>
  void Init(bool delete_savegame = true,
            const Savegame::ByteVector* initial_data = NULL) {
    // Destroy the current one first. We can't have two VFSs with the same name
    // concurrently.
    storage_manager_.reset(NULL);

    scoped_ptr<StorageManager::UpgradeHandler> upgrade_handler(
        new MockUpgradeHandler());
    StorageManager::Options options;
    options.savegame_options.delete_on_destruction = delete_savegame;
    options.savegame_options.factory = &SavegameFake::Create;
    if (initial_data) {
      options.savegame_options.test_initial_data = *initial_data;
    }
    storage_manager_.reset(
        new StorageManagerType(upgrade_handler.Pass(), options));
  }

  template <typename StorageManagerType>
  void FinishIO() {
    storage_manager_->FinishIO();
  }

  MessageLoop message_loop_;
  scoped_ptr<StorageManager> storage_manager_;
};

TEST_F(StorageManagerTest, ObtainConnection) {
  // Verify that the SQL connection is non-null.
  Init<StorageManager>();
  SqlWaiter waiter;
  storage_manager_->GetSqlContext(
      base::Bind(&SqlWaiter::OnSqlConnection, base::Unretained(&waiter)));
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(waiter.TimedWait());
}

TEST_F(StorageManagerTest, QuerySchemaVersion) {
  Init<StorageManager>(false /* delete_savegame */);
  storage_manager_->GetSqlContext(base::Bind(&QuerySchemaCallback));
  message_loop_.RunUntilIdle();

  // Force a write to disk and wait until it's done.
  FlushWaiter waiter;
  storage_manager_->FlushNow(
      base::Bind(&FlushWaiter::OnFlushDone, base::Unretained(&waiter)));
  EXPECT_TRUE(waiter.TimedWait());

  Init<StorageManager>();
  storage_manager_->GetSqlContext(base::Bind(&InspectSchemaVersionCallback));
  message_loop_.RunUntilIdle();
}

TEST_F(StorageManagerTest, FlushNow) {
  // Ensure the Flush callback is called.
  Init<StorageManager>();
  storage_manager_->GetSqlContext(base::Bind(&FlushCallback));
  message_loop_.RunUntilIdle();
  FlushWaiter waiter;
  storage_manager_->FlushNow(
      base::Bind(&FlushWaiter::OnFlushDone, base::Unretained(&waiter)));
  EXPECT_TRUE(waiter.TimedWait());
}

TEST_F(StorageManagerTest, FlushNowWithFlushOnChange) {
  // Test that the Flush callback is called exactly once, despite calling both
  // FlushOnChange() and FlushNow().
  Init<MockStorageManager>();

  storage_manager_->GetSqlContext(base::Bind(&FlushCallback));
  message_loop_.RunUntilIdle();

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

  storage_manager_->GetSqlContext(base::Bind(&FlushCallback));
  message_loop_.RunUntilIdle();

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

  storage_manager_->GetSqlContext(base::Bind(&FlushCallback));
  message_loop_.RunUntilIdle();

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

  storage_manager_->GetSqlContext(base::Bind(&FlushCallback));
  message_loop_.RunUntilIdle();

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
  message_loop_.RunUntilIdle();
}

}  // namespace storage
}  // namespace cobalt
