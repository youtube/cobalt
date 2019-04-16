// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/browser/storage_upgrade_handler.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/storage_area.h"
#include "cobalt/loader/origin.h"
#include "cobalt/network/persistent_cookie_store.h"
#include "cobalt/storage/savegame_fake.h"
#include "cobalt/storage/storage_manager.h"
#include "cobalt/storage/upgrade/upgrade_reader.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace browser {

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

class CookieWaiter : public CallbackWaiter {
 public:
  CookieWaiter() {}
  ~CookieWaiter() {}

  void OnCookiesLoaded(
      std::vector<std::unique_ptr<net::CanonicalCookie>> cookies) {
    cookies_ = std::move(cookies);
    Signal();
  }

  const std::vector<std::unique_ptr<net::CanonicalCookie>>& GetCookies() const {
    return cookies_;
  }

 private:
  std::vector<std::unique_ptr<net::CanonicalCookie>> cookies_;
  DISALLOW_COPY_AND_ASSIGN(CookieWaiter);
};

class LocalStorageEntryWaiter : public CallbackWaiter {
 public:
  LocalStorageEntryWaiter() {}
  ~LocalStorageEntryWaiter() {}

  void OnEntriesLoaded(std::unique_ptr<dom::StorageArea::StorageMap> entries) {
    entries_ = std::move(entries);
    Signal();
  }

  dom::StorageArea::StorageMap* GetEntries() const { return entries_.get(); }

 private:
  std::unique_ptr<dom::StorageArea::StorageMap> entries_;
  DISALLOW_COPY_AND_ASSIGN(LocalStorageEntryWaiter);
};

void ReadFileToString(const char* pathname, std::string* string_out) {
  EXPECT_TRUE(pathname);
  EXPECT_TRUE(string_out);
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEST_DATA, &file_path));
  file_path = file_path.Append(pathname);
  EXPECT_TRUE(base::ReadFileToString(file_path, string_out));
  const char* data = string_out->c_str();
  const int size = static_cast<int>(string_out->length());
  EXPECT_GT(size, 0);
  EXPECT_LE(size, 10 * 1024 * 1024);
  EXPECT_TRUE(storage::upgrade::UpgradeReader::IsUpgradeData(data, size));
}

int GetNumCookies(storage::StorageManager* storage,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  scoped_refptr<network::PersistentCookieStore> cookie_store(
      new network::PersistentCookieStore(storage, task_runner));
  CookieWaiter waiter;
  cookie_store->Load(
      base::Bind(&CookieWaiter::OnCookiesLoaded, base::Unretained(&waiter)),
      net::NetLogWithSource());
  EXPECT_EQ(true, waiter.TimedWait());
  return static_cast<int>(waiter.GetCookies().size());
}

int GetNumLocalStorageEntries(storage::StorageManager* storage,
                              const loader::Origin& origin) {
  dom::LocalStorageDatabase local_storage_database(storage);
  LocalStorageEntryWaiter waiter;
  local_storage_database.ReadAll(
      origin, base::Bind(&LocalStorageEntryWaiter::OnEntriesLoaded,
                         base::Unretained(&waiter)));
  EXPECT_EQ(true, waiter.TimedWait());
  return static_cast<int>(waiter.GetEntries()->size());
}

}  // namespace

TEST(StorageUpgradeHandlerTest, UpgradeFullData) {
  base::MessageLoop message_loop_(base::MessageLoop::TYPE_DEFAULT);
  // Cookie loaded callback will be posted to the separate thread to enable
  // synchronous test logic.
  base::Thread separate_thread("Cookie Callback");
  base::Thread::Options thread_options;
  thread_options.priority = base::ThreadPriority::HIGHEST;
  separate_thread.StartWithOptions(thread_options);
  std::string file_contents;
  ReadFileToString("cobalt/storage/upgrade/testdata/full_data_v1.json",
                   &file_contents);
  StorageUpgradeHandler* upgrade_handler = new StorageUpgradeHandler(
      GURL("https://www.youtube.com"), nullptr, separate_thread.task_runner());
  storage::StorageManager::Options options;
  options.savegame_options.delete_on_destruction = true;
  options.savegame_options.factory = &storage::SavegameFake::Create;
  storage::StorageManager storage(
      std::unique_ptr<storage::StorageManager::UpgradeHandler>(upgrade_handler),
      options);

  // Our storage should be empty at this point.
  EXPECT_EQ(GetNumCookies(&storage, separate_thread.task_runner()), 0);
  EXPECT_EQ(GetNumLocalStorageEntries(
                &storage, upgrade_handler->default_local_storage_origin()),
            0);

  upgrade_handler->OnUpgrade(&storage, file_contents.c_str(),
                             static_cast<int>(file_contents.length()));

  FlushWaiter waiter;
  storage.FlushNow(
      base::Bind(&FlushWaiter::OnFlushDone, base::Unretained(&waiter)));
  EXPECT_EQ(true, waiter.TimedWait());

  // We should now have 2 cookies and 2 local storage entries.
  EXPECT_EQ(GetNumCookies(&storage, separate_thread.task_runner()), 2);
  EXPECT_EQ(GetNumLocalStorageEntries(
                &storage, upgrade_handler->default_local_storage_origin()),
            2);

  base::RunLoop().RunUntilIdle();
}

}  // namespace browser
}  // namespace cobalt
