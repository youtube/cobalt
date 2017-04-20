// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/storage_upgrade_handler.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/storage_area.h"
#include "cobalt/network/persistent_cookie_store.h"
#include "cobalt/storage/storage_manager.h"
#include "cobalt/storage/upgrade/upgrade_reader.h"
#include "net/cookies/canonical_cookie.h"

namespace cobalt {
namespace browser {
namespace {
void OnCookiesLoaded(const std::vector<net::CanonicalCookie*>& cookies) {
  // We don't care about the local copies of the cookies returned to us.
  for (size_t i = 0; i < cookies.size(); i++) {
    delete cookies[i];
  }
}
}  // namespace

StorageUpgradeHandler::StorageUpgradeHandler(const GURL& url)
    : default_local_storage_id_(
          dom::StorageArea::GetLocalStorageIdForUrl(url)) {}

void StorageUpgradeHandler::OnUpgrade(storage::StorageManager* storage,
                                      const char* data, int size) {
  storage::upgrade::UpgradeReader upgrade_reader;
  upgrade_reader.Parse(data, size);
  int num_cookies = upgrade_reader.GetNumCookies();
  int num_local_storage_entries = upgrade_reader.GetNumLocalStorageEntries();
  DLOG(INFO) << "Upgrading legacy save data: " << num_cookies << " cookies, "
             << num_local_storage_entries << " local storage entries.";

  if (num_cookies > 0) {
    scoped_refptr<network::PersistentCookieStore> cookie_store(
        new network::PersistentCookieStore(storage));
    // Load the current cookies to ensure the database table is initialized.
    cookie_store->Load(base::Bind(OnCookiesLoaded));
    for (int i = 0; i < num_cookies; i++) {
      const net::CanonicalCookie* cookie = upgrade_reader.GetCookie(i);
      DCHECK(cookie);
      cookie_store->AddCookie(*cookie);
    }
  }

  if (num_local_storage_entries > 0) {
    dom::LocalStorageDatabase local_storage_database(storage);
    for (int i = 0; i < num_local_storage_entries; i++) {
      const storage::upgrade::UpgradeReader::LocalStorageEntry*
          local_storage_entry = upgrade_reader.GetLocalStorageEntry(i);
      DCHECK(local_storage_entry);
      local_storage_database.Write(default_local_storage_id_,
                                   local_storage_entry->key,
                                   local_storage_entry->value);
    }
  }
}

}  // namespace browser
}  // namespace cobalt
