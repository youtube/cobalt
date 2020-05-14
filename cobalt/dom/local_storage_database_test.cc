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

#include "cobalt/dom/local_storage_database.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/storage/savegame_fake.h"
#include "cobalt/storage/storage_manager.h"

#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

namespace {

class CallbackWaiter {
 public:
  CallbackWaiter()
      : was_called_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  virtual ~CallbackWaiter() {}
  bool TimedWait() {
    return was_called_event_.TimedWait(base::TimeDelta::FromSeconds(5));
  }

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

class Reader : public CallbackWaiter {
 public:
  Reader() {}
  ~Reader() {}

  void OnReadAll(std::unique_ptr<StorageArea::StorageMap> data) {
    data_ = *data;
    Signal();
  }
  StorageArea::StorageMap data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Reader);
};

std::string GetSavePath() {
  base::FilePath test_path;
  CHECK(base::PathService::Get(paths::DIR_COBALT_TEST_OUT, &test_path));
  return test_path.Append("local_storage_database_test.bin").value();
}

class DummyUpgradeHandler : public storage::StorageManager::UpgradeHandler {
  void OnUpgrade(storage::StorageManager* storage, const char* data,
                 int size) override {}
};

class LocalStorageDatabaseTest : public ::testing::Test {
 protected:
  LocalStorageDatabaseTest()
      : message_loop_(base::MessageLoop::TYPE_DEFAULT),
        origin_(GURL("https://www.example.com")) {
    std::unique_ptr<storage::StorageManager::UpgradeHandler> upgrade_handler(
        new DummyUpgradeHandler());
    storage::StorageManager::Options options;
    options.savegame_options.path_override = GetSavePath();
    options.savegame_options.delete_on_destruction = true;
    options.savegame_options.factory = &storage::SavegameFake::Create;

    storage_manager_.reset(
        new storage::StorageManager(std::move(upgrade_handler), options));
    db_.reset(new LocalStorageDatabase(storage_manager_.get()));
  }

  ~LocalStorageDatabaseTest() {
    db_.reset();
    storage_manager_.reset();
  }

  base::MessageLoop message_loop_;
  loader::Origin origin_;
  std::unique_ptr<storage::StorageManager> storage_manager_;
  std::unique_ptr<LocalStorageDatabase> db_;
};
}  // namespace

TEST_F(LocalStorageDatabaseTest, EmptyRead) {
  StorageArea::StorageMap empty;
  Reader reader;
  db_->ReadAll(origin_,
               base::Bind(&Reader::OnReadAll, base::Unretained(&reader)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(reader.TimedWait());
  EXPECT_EQ(empty, reader.data_);
}

TEST_F(LocalStorageDatabaseTest, WritePersists) {
  StorageArea::StorageMap test_vals;
  test_vals["key0"] = "value0";
  test_vals["key1"] = "value1";

  for (StorageArea::StorageMap::const_iterator it = test_vals.begin();
       it != test_vals.end(); ++it) {
    db_->Write(origin_, it->first, it->second);
  }

  FlushWaiter waiter;
  db_->Flush(base::Bind(&FlushWaiter::OnFlushDone, base::Unretained(&waiter)));
  EXPECT_EQ(true, waiter.TimedWait());

  // Ensure a Flush persists the data.
  db_.reset(new LocalStorageDatabase(storage_manager_.get()));
  Reader reader;
  db_->ReadAll(origin_,
               base::Bind(&Reader::OnReadAll, base::Unretained(&reader)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(reader.TimedWait());
  EXPECT_EQ(test_vals, reader.data_);
}

TEST_F(LocalStorageDatabaseTest, Delete) {
  StorageArea::StorageMap test_vals;
  test_vals["key0"] = "value0";
  test_vals["key1"] = "value1";

  for (StorageArea::StorageMap::const_iterator it = test_vals.begin();
       it != test_vals.end(); ++it) {
    db_->Write(origin_, it->first, it->second);
  }

  db_->Delete(origin_, "key0");
  StorageArea::StorageMap expected_vals;
  expected_vals["key1"] = "value1";
  Reader reader;
  db_->ReadAll(origin_,
               base::Bind(&Reader::OnReadAll, base::Unretained(&reader)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(reader.TimedWait());
  EXPECT_EQ(expected_vals, reader.data_);
}

TEST_F(LocalStorageDatabaseTest, Clear) {
  StorageArea::StorageMap test_vals;
  test_vals["key0"] = "value0";
  test_vals["key1"] = "value1";

  for (StorageArea::StorageMap::const_iterator it = test_vals.begin();
       it != test_vals.end(); ++it) {
    db_->Write(origin_, it->first, it->second);
  }
  db_->Clear(origin_);
  StorageArea::StorageMap expected_vals;
  Reader reader;
  db_->ReadAll(origin_,
               base::Bind(&Reader::OnReadAll, base::Unretained(&reader)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(reader.TimedWait());
  EXPECT_EQ(expected_vals, reader.data_);
}
}  // namespace dom
}  // namespace cobalt
