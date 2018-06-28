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

#include <vector>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace network {

namespace {
const base::TimeDelta kMaxCookieLifetime = base::TimeDelta::FromDays(365 * 2);

std::vector<net::CanonicalCookie*> GetAllCookies(
    const storage::MemoryStore& memory_store) {
  std::vector<net::CanonicalCookie*> actual_cookies;

  memory_store.GetAllCookies(&actual_cookies);
  return actual_cookies;
}

void CookieStorageInit(
    const PersistentCookieStore::LoadedCallback& loaded_callback,
    const storage::MemoryStore& memory_store) {
  TRACE_EVENT0("cobalt::network", "PersistentCookieStore::CookieStorageInit()");
  loaded_callback.Run(GetAllCookies(memory_store));
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
    const storage::MemoryStore& memory_store) {
  UNREFERENCED_PARAMETER(memory_store);
  std::vector<net::CanonicalCookie*> empty_cookie_list;
  loaded_callback.Run(empty_cookie_list);
}

}  // namespace

PersistentCookieStore::PersistentCookieStore(storage::StorageManager* storage)
    : storage_(storage) {}

PersistentCookieStore::~PersistentCookieStore() {}

void PersistentCookieStore::Load(const LoadedCallback& loaded_callback) {
  storage_->WithReadOnlyMemoryStore(
      base::Bind(&CookieStorageInit, loaded_callback));
}

void PersistentCookieStore::LoadCookiesForKey(
    const std::string& key, const LoadedCallback& loaded_callback) {
  UNREFERENCED_PARAMETER(key);
  // We don't support loading of individual cookies.
  // This is always called after Load(), so just post the callback to the
  // Storage thread to make sure it is run after Load() has finished. See
  // comments in net/cookie_monster.cc for more information.
  storage_->WithReadOnlyMemoryStore(
      base::Bind(&SendEmptyCookieList, loaded_callback));
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

void PersistentCookieStore::Flush(const base::Closure& callback) {
  storage_->FlushNow(callback);
}

}  // namespace network
}  // namespace cobalt
