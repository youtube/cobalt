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

#include <memory>

#include "cobalt/dom/local_storage_database.h"

#include "base/trace_event/trace_event.h"
#include "cobalt/dom/storage_area.h"
#include "cobalt/storage/storage_manager.h"
#include "cobalt/storage/store/memory_store.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace dom {

namespace {

void LocalStorageInit(const storage::MemoryStore& memory_store) {
  LOG(INFO) << "local_storage Init";
}

void LocalStorageReadValues(
    const loader::Origin& origin,
    const LocalStorageDatabase::ReadCompletionCallback& callback,
    const storage::MemoryStore& memory_store) {
  LOG(INFO) << "LocalStorageReadValues";
  TRACK_MEMORY_SCOPE("Storage");

  std::unique_ptr<storage::MemoryStore::LocalStorageMap> values(
      new storage::MemoryStore::LocalStorageMap);
  memory_store.ReadAllLocalStorage(origin, values.get());
  callback.Run(std::move(values));
}

void LocalStorageWrite(const loader::Origin& origin, const std::string& key,
                       const std::string& value,
                       storage::MemoryStore* memory_store) {
  TRACK_MEMORY_SCOPE("Storage");
  memory_store->WriteToLocalStorage(origin, key, value);
}

void LocalStorageDelete(const loader::Origin& origin, const std::string& key,
                        storage::MemoryStore* memory_store) {
  TRACK_MEMORY_SCOPE("Storage");
  memory_store->DeleteFromLocalStorage(origin, key);
}

void LocalStorageClear(const loader::Origin& origin,
                       storage::MemoryStore* memory_store) {
  TRACK_MEMORY_SCOPE("Storage");
  memory_store->ClearLocalStorage(origin);
}
}  // namespace

LocalStorageDatabase::LocalStorageDatabase(storage::StorageManager* storage)
    : storage_(storage), initialized_(false) {}

// Init is done lazily only once the first operation occurs. This is to avoid
// a potential wait while the storage manager loads from disk.
void LocalStorageDatabase::Init() {
  if (!initialized_) {
    storage_->WithReadOnlyMemoryStore(base::Bind(&LocalStorageInit));
    initialized_ = true;
  }
}

void LocalStorageDatabase::ReadAll(const loader::Origin& origin,
                                   const ReadCompletionCallback& callback) {
  TRACK_MEMORY_SCOPE("Storage");
  Init();
  storage_->WithReadOnlyMemoryStore(
      base::Bind(&LocalStorageReadValues, origin, callback));
}

void LocalStorageDatabase::Write(const loader::Origin& origin,
                                 const std::string& key,
                                 const std::string& value) {
  TRACK_MEMORY_SCOPE("Storage");
  Init();
  storage_->WithMemoryStore(base::Bind(&LocalStorageWrite, origin, key, value));
}

void LocalStorageDatabase::Delete(const loader::Origin& origin,
                                  const std::string& key) {
  TRACK_MEMORY_SCOPE("Storage");
  Init();
  storage_->WithMemoryStore(base::Bind(&LocalStorageDelete, origin, key));
}

void LocalStorageDatabase::Clear(const loader::Origin& origin) {
  TRACK_MEMORY_SCOPE("Storage");
  Init();
  storage_->WithMemoryStore(base::Bind(&LocalStorageClear, origin));
}

void LocalStorageDatabase::Flush(const base::Closure& callback) {
  TRACK_MEMORY_SCOPE("Storage");
  storage_->FlushNow(callback);
}

}  // namespace dom
}  // namespace cobalt
