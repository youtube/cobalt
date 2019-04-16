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

#include "cobalt/dom/storage_area.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/storage.h"
#include "cobalt/storage/store/memory_store.h"

namespace cobalt {
namespace dom {

StorageArea::StorageArea(Storage* storage_node, LocalStorageDatabase* db)
    : read_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                  base::WaitableEvent::InitialState::NOT_SIGNALED),
      storage_node_(storage_node),
      size_bytes_(0),
      db_interface_(db),
      initialized_(false) {}

StorageArea::~StorageArea() {}

int StorageArea::length() {
  Init();
  return static_cast<int>(storage_map_->size());
}

base::Optional<std::string> StorageArea::Key(int index) {
  Init();

  if (index < 0 || index >= length()) {
    return base::nullopt;
  }
  // Advance N elements to get to index.
  // TODO: If this is called often, we should cache the iterator.
  StorageMap::const_iterator it = storage_map_->begin();
  int count = 0;
  while (count < index) {
    ++it;
    ++count;
  }
  return it->first;
}

base::Optional<std::string> StorageArea::GetItem(const std::string& key) {
  Init();

  StorageMap::const_iterator it = storage_map_->find(key);
  if (it != storage_map_->end()) {
    return it->second;
  } else {
    return base::nullopt;
  }
}

void StorageArea::SetItem(const std::string& key, const std::string& value) {
  Init();

  base::Optional<std::string> old_value = GetItem(key);

  // If the previous value is equal to value, then the method must do nothing.
  // https://www.w3.org/TR/2015/CR-webstorage-20150609/#storage-0
  if (old_value == value) {
    return;
  }

  // TODO: Implement quota handling.
  size_bytes_ +=
      static_cast<int>(value.length() - old_value.value_or("").length());
  (*storage_map_)[key] = value;
  storage_node_->DispatchEvent(key, old_value, value);
  if (db_interface_) {
    db_interface_->Write(origin_, key, value);
  }
}

void StorageArea::RemoveItem(const std::string& key) {
  Init();

  StorageMap::iterator it = storage_map_->find(key);

  // If no item with this key exists, the method must do nothing.
  // https://www.w3.org/TR/2015/CR-webstorage-20150609/#storage-0
  if (it == storage_map_->end()) {
    return;
  }

  size_bytes_ -= static_cast<int>(it->second.length());
  std::string old_value = it->second;
  storage_map_->erase(it);
  storage_node_->DispatchEvent(key, old_value, base::nullopt);
  if (db_interface_) {
    db_interface_->Delete(origin_, key);
  }
}

void StorageArea::Clear() {
  Init();

  storage_map_->clear();
  size_bytes_ = 0;
  storage_node_->DispatchEvent(base::nullopt, base::nullopt, base::nullopt);
  if (db_interface_) {
    db_interface_->Clear(origin_);
  }
}

bool StorageArea::key_exists(const std::string& key_name) {
  Init();
  return storage_map_->find(key_name) != storage_map_->end();
}

void StorageArea::Init() {
  if (initialized_) {
    return;
  }

  origin_ = loader::Origin(storage_node_->origin());

  if (db_interface_) {
    // LocalStorage path. We read our StorageMap from the database.
    // Do a one-time read from the database for what's currently in
    // LocalStorage.
    db_interface_->ReadAll(origin_, base::Bind(&StorageArea::OnInitComplete,
                                               base::Unretained(this)));
    read_event_.Wait();
  } else {
    // SessionStorage. Create a new, empty StorageMap.
    storage_map_.reset(new StorageMap);
  }
  initialized_ = true;
}

void StorageArea::OnInitComplete(std::unique_ptr<StorageMap> data) {
  storage_map_.reset(data.release());
  read_event_.Signal();
}

}  // namespace dom
}  // namespace cobalt
