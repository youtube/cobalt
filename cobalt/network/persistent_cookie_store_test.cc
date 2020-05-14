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

#include "cobalt/network/persistent_cookie_store.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/storage/savegame.h"
#include "cobalt/storage/savegame_fake.h"
#include "cobalt/storage/storage_manager.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace network {

namespace {

std::unique_ptr<net::CanonicalCookie> CreateTestCookie() {
  GURL url("http://www.example.com/test");
  base::Time current_time = base::Time::Now();
  base::Time expiration_time = current_time + base::TimeDelta::FromDays(1);
  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateSanitizedCookie(
          url, "A", "2", "www.example.com", "/test", current_time,
          expiration_time, base::Time(), false, false,
          net::CookieSameSite::DEFAULT_MODE, net::COOKIE_PRIORITY_DEFAULT));
  return cookie;
}

void DummyOnLoaded(std::vector<std::unique_ptr<net::CanonicalCookie>> cookies) {
}

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

class CookieLoader : public CallbackWaiter {
 public:
  CookieLoader() {}
  ~CookieLoader() {}

  void OnCookieLoad(
      std::vector<std::unique_ptr<net::CanonicalCookie>> loaded_cookies) {
    cookies = std::move(loaded_cookies);
    Signal();
  }
  std::vector<std::unique_ptr<net::CanonicalCookie>> cookies;

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieLoader);
};

class DummyUpgradeHandler : public storage::StorageManager::UpgradeHandler {
  void OnUpgrade(storage::StorageManager* storage, const char* data,
                 int size) override {}
};

std::string GetSavePath() {
  base::FilePath test_path;
  CHECK(base::PathService::Get(paths::DIR_COBALT_TEST_OUT, &test_path));
  return test_path.Append("persistent_cookie_test.bin").value();
}

class PersistentCookieStoreTest : public ::testing::Test {
 protected:
  PersistentCookieStoreTest() : message_loop_(base::MessageLoop::TYPE_DEFAULT) {
    std::unique_ptr<storage::StorageManager::UpgradeHandler> upgrade_handler(
        new DummyUpgradeHandler());
    storage::StorageManager::Options options;
    options.savegame_options.path_override = GetSavePath();
    options.savegame_options.delete_on_destruction = true;
    options.savegame_options.factory = &storage::SavegameFake::Create;

    storage_manager_.reset(
        new storage::StorageManager(std::move(upgrade_handler), options));
    cookie_store_ = new PersistentCookieStore(
        storage_manager_.get(), base::MessageLoop::current()->task_runner());
  }

  ~PersistentCookieStoreTest() {
    cookie_store_ = NULL;
    storage_manager_.reset(NULL);
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<storage::StorageManager> storage_manager_;
  scoped_refptr<PersistentCookieStore> cookie_store_;
};
}  // namespace

TEST_F(PersistentCookieStoreTest, LoadGetsAddedCookies) {
  // Put a cookie into the database. Flush, then reload and make sure
  // the Loaded callback contains it.
  std::unique_ptr<net::CanonicalCookie> test_cookie(CreateTestCookie());
  cookie_store_->Load(base::Bind(&DummyOnLoaded), net::NetLogWithSource());
  cookie_store_->AddCookie(*test_cookie);
  FlushWaiter waiter;
  cookie_store_->Flush(
      base::Bind(&FlushWaiter::OnFlushDone, base::Unretained(&waiter)));
  EXPECT_EQ(true, waiter.TimedWait());

  // We OnCookieLoaded will be posted to the separate thread to enable
  // synchronous test logic.
  base::Thread separate_thread("Cookie Callback");
  base::Thread::Options thread_options;
  thread_options.priority = base::ThreadPriority::HIGHEST;
  separate_thread.StartWithOptions(thread_options);
  cookie_store_ = new PersistentCookieStore(storage_manager_.get(),
                                            separate_thread.task_runner());
  CookieLoader loader;
  cookie_store_->Load(
      base::Bind(&CookieLoader::OnCookieLoad, base::Unretained(&loader)),
      net::NetLogWithSource());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(loader.TimedWait());
  ASSERT_EQ(1, loader.cookies.size());
  EXPECT_TRUE(test_cookie->IsEquivalent(*loader.cookies[0]));
}
}  // namespace network
}  // namespace cobalt
