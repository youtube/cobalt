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

#include "cobalt/network/persistent_cookie_store.h"

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/storage/savegame.h"
#include "cobalt/storage/savegame_fake.h"
#include "cobalt/storage/storage_manager.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/canonical_cookie.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace network {

namespace {

scoped_ptr<net::CanonicalCookie> CreateTestCookie() {
  GURL url("http://www.example.com/test");
  base::Time current_time = base::Time::Now();
  base::Time expiration_time = current_time + base::TimeDelta::FromDays(1);
  scoped_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
      url, "A", "2", "www.example.com", "/test", "", "", current_time,
      expiration_time, false, false));
  return cookie.Pass();
}

void DummyOnLoaded(const std::vector<net::CanonicalCookie*>& cookies) {
  UNREFERENCED_PARAMETER(cookies);
}

class CallbackWaiter {
 public:
  CallbackWaiter() : was_called_event_(true, false) {}
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
  ~CookieLoader() {
    for (std::vector<net::CanonicalCookie*>::iterator it = cookies.begin();
         it != cookies.end(); ++it) {
      delete *it;
    }
  }

  void OnCookieLoad(const std::vector<net::CanonicalCookie*>& loaded_cookies) {
    cookies = loaded_cookies;
    Signal();
  }
  std::vector<net::CanonicalCookie*> cookies;

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieLoader);
};

class CookieVerifier : public CallbackWaiter {
 public:
  explicit CookieVerifier(net::CanonicalCookie* test_cookie)
      : test_cookie(test_cookie) {}
  void SqlSelect(storage::SqlContext* sql_context) {
    std::string statement =
        base::StringPrintf("SELECT url FROM CookieTable WHERE url = \"%s\";",
                           test_cookie->Source().c_str());
    sql::Statement get_cookie(
        sql_context->sql_connection()->GetUniqueStatement(statement.c_str()));
    EXPECT_EQ(true, get_cookie.Step());
    EXPECT_EQ(test_cookie->Source(), get_cookie.ColumnString(0));
    Signal();
  }

  net::CanonicalCookie* test_cookie;

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieVerifier);
};

class DummyUpgradeHandler : public storage::StorageManager::UpgradeHandler {
  void OnUpgrade(storage::StorageManager* /*storage*/, const char* /*data*/,
                 int /*size*/) override {}
};

std::string GetSavePath() {
  FilePath test_path;
  CHECK(PathService::Get(paths::DIR_COBALT_TEST_OUT, &test_path));
  return test_path.Append("persistent_cookie_test.bin").value();
}

class PersistentCookieStoreTest : public ::testing::Test {
 protected:
  PersistentCookieStoreTest() : message_loop_(MessageLoop::TYPE_DEFAULT) {
    scoped_ptr<storage::StorageManager::UpgradeHandler> upgrade_handler(
        new DummyUpgradeHandler());
    storage::StorageManager::Options options;
    options.savegame_options.path_override = GetSavePath();
    options.savegame_options.delete_on_destruction = true;
    options.savegame_options.factory = &storage::SavegameFake::Create;

    storage_manager_.reset(
        new storage::StorageManager(upgrade_handler.Pass(), options));
    cookie_store_ = new PersistentCookieStore(storage_manager_.get());
  }

  ~PersistentCookieStoreTest() {
    cookie_store_ = NULL;
    storage_manager_.reset(NULL);
  }

  MessageLoop message_loop_;
  scoped_ptr<storage::StorageManager> storage_manager_;
  scoped_refptr<PersistentCookieStore> cookie_store_;
};
}  // namespace

TEST_F(PersistentCookieStoreTest, LoadGetsCookies) {
  // Put a cookie into the database. Flush, then reload and make sure
  // the Loaded callback contains it.
  scoped_ptr<net::CanonicalCookie> test_cookie(CreateTestCookie());
  cookie_store_->Load(base::Bind(&DummyOnLoaded));
  cookie_store_->AddCookie(*test_cookie);
  FlushWaiter waiter;
  cookie_store_->Flush(
      base::Bind(&FlushWaiter::OnFlushDone, base::Unretained(&waiter)));
  EXPECT_EQ(true, waiter.TimedWait());

  cookie_store_ = new PersistentCookieStore(storage_manager_.get());
  CookieLoader loader;
  cookie_store_->Load(
      base::Bind(&CookieLoader::OnCookieLoad, base::Unretained(&loader)));
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(loader.TimedWait());
  ASSERT_EQ(1, loader.cookies.size());
  EXPECT_TRUE(test_cookie->IsEquivalent(*loader.cookies[0]));
}

TEST_F(PersistentCookieStoreTest, AddCookie) {
  CookieLoader loader;
  cookie_store_->Load(
      base::Bind(&CookieLoader::OnCookieLoad, base::Unretained(&loader)));
  message_loop_.RunUntilIdle();

  scoped_ptr<net::CanonicalCookie> test_cookie(CreateTestCookie());
  cookie_store_->AddCookie(*test_cookie);

  CookieVerifier verifier(test_cookie.get());
  storage_manager_->GetSqlContext(
      base::Bind(&CookieVerifier::SqlSelect, base::Unretained(&verifier)));
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(verifier.TimedWait());
}

}  // namespace network
}  // namespace cobalt
