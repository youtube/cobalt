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

#include "cobalt/dom/local_storage_database.h"

#include "base/debug/trace_event.h"
#include "cobalt/dom/storage_area.h"
#include "cobalt/storage/storage_manager.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace dom {

namespace {

void LocalStorageInit(const storage::MemoryStore& memory_store) {
  LOG(INFO) << "local_storage Init";
  UNREFERENCED_PARAMETER(memory_store);
}

void LocalStorageReadValues(
    const std::string& id,
    const LocalStorageDatabase::ReadCompletionCallback& callback,
    const storage::MemoryStore& memory_store) {
  LOG(INFO) << "LocalStorageReadValues";
  TRACK_MEMORY_SCOPE("Storage");

  scoped_ptr<storage::MemoryStore::LocalStorageMap> values(
      new storage::MemoryStore::LocalStorageMap);
  memory_store.ReadAllLocalStorage(id, values.get());
  callback.Run(values.Pass());
}

void LocalStorageWrite(const std::string& id, const std::string& key,
                       const std::string& value,
                       storage::MemoryStore* memory_store) {
  TRACK_MEMORY_SCOPE("Storage");
  memory_store->WriteToLocalStorage(id, key, value);
}

void LocalStorageDelete(const std::string& id, const std::string& key,
                        storage::MemoryStore* memory_store) {
  TRACK_MEMORY_SCOPE("Storage");
  memory_store->DeleteFromLocalStorage(id, key);
}

void LocalStorageClear(const std::string& id,
                       storage::MemoryStore* memory_store) {
  TRACK_MEMORY_SCOPE("Storage");
  memory_store->ClearLocalStorage(id);
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

void LocalStorageDatabase::ReadAll(const std::string& id,
                                   const ReadCompletionCallback& callback) {
  TRACK_MEMORY_SCOPE("Storage");
  Init();
  storage_->WithReadOnlyMemoryStore(
      base::Bind(&LocalStorageReadValues, id, callback));
}

void LocalStorageDatabase::Write(const std::string& id, const std::string& key,
                                 const std::string& value) {
  TRACK_MEMORY_SCOPE("Storage");
  Init();
  storage_->WithMemoryStore(base::Bind(&LocalStorageWrite, id, key, value));
}

void LocalStorageDatabase::Delete(const std::string& id,
                                  const std::string& key) {
  TRACK_MEMORY_SCOPE("Storage");
  Init();
  storage_->WithMemoryStore(base::Bind(&LocalStorageDelete, id, key));
}

void LocalStorageDatabase::Clear(const std::string& id) {
  TRACK_MEMORY_SCOPE("Storage");
  Init();
  storage_->WithMemoryStore(base::Bind(&LocalStorageClear, id));
}

void LocalStorageDatabase::Flush(const base::Closure& callback) {
  TRACK_MEMORY_SCOPE("Storage");
  storage_->FlushNow(callback);
}

}  // namespace dom
}  // namespace cobalt
