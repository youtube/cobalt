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

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "url/gurl.h"

namespace cobalt {
namespace network {

namespace {
const base::TimeDelta kMaxCookieLifetime = base::TimeDelta::FromDays(365 * 2);

void CookieStorageInit(
    const PersistentCookieStore::LoadedCallback& loaded_callback,
    scoped_refptr<base::SingleThreadTaskRunner> loaded_callback_task_runner,
    const storage::MemoryStore& memory_store) {
  TRACE_EVENT0("cobalt::network", "PersistentCookieStore::CookieStorageInit()");

  std::vector<std::unique_ptr<net::CanonicalCookie>> actual_cookies;
  memory_store.GetAllCookies(&actual_cookies);

  DCHECK(loaded_callback_task_runner);
  if (!loaded_callback.is_null()) {
    loaded_callback_task_runner->PostTask(
        FROM_HERE,
        base::Bind(
            [](const PersistentCookieStore::LoadedCallback& loaded_callback,
               std::vector<std::unique_ptr<net::CanonicalCookie>> cookies) {
              loaded_callback.Run(std::move(cookies));
            },
            loaded_callback, base::Passed(&actual_cookies)));
  }
}

void CookieStorageAddCookie(const net::CanonicalCookie& cc,
                            storage::MemoryStore* memory_store) {
  base::Time maximum_expiry = base::Time::Now() + kMaxCookieLifetime;
  base::Time expiry = cc.ExpiryDate();
  if (expiry > maximum_expiry) {
    expiry = maximum_expiry;
  }

  memory_store->AddCookie(cc, expiry.ToInternalValue());
}

void CookieStorageCookieAccessTime(const net::CanonicalCookie& cc,
                                   storage::MemoryStore* memory_store) {
  base::Time maximum_expiry = base::Time::Now() + kMaxCookieLifetime;
  base::Time expiry = cc.ExpiryDate();
  if (expiry > maximum_expiry) {
    expiry = maximum_expiry;
  }

  memory_store->UpdateCookieAccessTime(cc, expiry.ToInternalValue());
}

void CookieStorageDeleteCookie(const net::CanonicalCookie& cc,
                               storage::MemoryStore* memory_store) {
  memory_store->DeleteCookie(cc);
}

void SendEmptyCookieList(
    const PersistentCookieStore::LoadedCallback& loaded_callback,
    scoped_refptr<base::SingleThreadTaskRunner> loaded_callback_task_runner,
    const storage::MemoryStore& memory_store) {
  loaded_callback_task_runner->PostTask(
      FROM_HERE,
      base::Bind(
          [](const PersistentCookieStore::LoadedCallback& loaded_callback) {
            std::vector<std::unique_ptr<net::CanonicalCookie>>
                empty_cookie_list;
            loaded_callback.Run(std::move(empty_cookie_list));
          },
          loaded_callback));
}

}  // namespace

PersistentCookieStore::PersistentCookieStore(
    storage::StorageManager* storage,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : storage_(storage), loaded_callback_task_runner_(network_task_runner) {}

PersistentCookieStore::~PersistentCookieStore() {}

void PersistentCookieStore::Load(const LoadedCallback& loaded_callback,
                                 const net::NetLogWithSource& net_log) {
  net_log.BeginEvent(net::NetLogEventType::COOKIE_PERSISTENT_STORE_LOAD);
  //  DCHECK_EQ(base::MessageLoop::current()->task_runner(),
  //            loaded_callback_task_runner_);
  storage_->WithReadOnlyMemoryStore(base::Bind(
      &CookieStorageInit, loaded_callback, loaded_callback_task_runner_));
}

void PersistentCookieStore::LoadCookiesForKey(
    const std::string& key, const LoadedCallback& loaded_callback) {
  // We don't support loading of individual cookies.
  // This is always called after Load(), so just post the callback to the
  // Storage thread to make sure it is run after Load() has finished. See
  // comments in net/cookie_monster.cc for more information.
  DCHECK_EQ(base::MessageLoop::current()->task_runner(),
            loaded_callback_task_runner_);
  storage_->WithReadOnlyMemoryStore(base::Bind(
      &SendEmptyCookieList, loaded_callback, loaded_callback_task_runner_));
}

void PersistentCookieStore::AddCookie(const net::CanonicalCookie& cc) {
  // We expect that all cookies we are fed are meant to persist.
  DCHECK(cc.IsPersistent());
  storage_->WithMemoryStore(base::Bind(&CookieStorageAddCookie, cc));
}

void PersistentCookieStore::UpdateCookieAccessTime(
    const net::CanonicalCookie& cc) {
  storage_->WithMemoryStore(base::Bind(&CookieStorageCookieAccessTime, cc));
}

void PersistentCookieStore::DeleteCookie(const net::CanonicalCookie& cc) {
  storage_->WithMemoryStore(base::Bind(&CookieStorageDeleteCookie, cc));
}

void PersistentCookieStore::SetForceKeepSessionState() {
  // We don't expect this to be called, and we don't implement these semantics.
  NOTREACHED();
}

void PersistentCookieStore::SetBeforeFlushCallback(
    base::RepeatingClosure callback) {
  NOTIMPLEMENTED();
}

void PersistentCookieStore::Flush(base::OnceClosure callback) {
  storage_->FlushNow(std::move(callback));
}

}  // namespace network
}  // namespace cobalt
