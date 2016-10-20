/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/storage_area.h"

#include "base/bind.h"
#include "base/stringprintf.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/storage.h"

namespace cobalt {
namespace dom {

namespace {
const char kLocalStorageSuffix[] = ".localstorage";
const char kSessionStorageSuffix[] = ".sessionstorage";

std::string OriginToDatabaseIdentifier(const GURL& url) {
  // For compatibility with existing saved data, this tries to behave the same
  // as WebCore's SecurityOrigin::databaseIdentifier().
  // e.g. https://www.youtube.com/tv should be converted to
  // https_www.youtube.com_0

  // NOTE: WebCore passes the encoded host through FileSystem's
  // encodeForFilename(). We assume our host will be well-formed.
  std::string encoded_host = url.HostNoBrackets();
  int port = url.IntPort();
  if (port == -1) {
    // A default/unspecified port in googleurl is -1, but Steel used KURL
    // from WebCore where a default port is 0. Convert to 0 for compatibility.
    port = 0;
  }

  return base::StringPrintf("%s_%s_%d", url.scheme().c_str(),
                            encoded_host.c_str(), port);
}
}  // namespace

StorageArea::StorageArea(Storage* storage_node, LocalStorageDatabase* db)
    : read_event_(true, false),
      storage_node_(storage_node),
      size_bytes_(0),
      db_interface_(db),
      initialized_(false) {
}

StorageArea::~StorageArea() {}

int StorageArea::length() {
  Init();
  return static_cast<int>(storage_map_->size());
}

base::optional<std::string> StorageArea::Key(int index) {
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

base::optional<std::string> StorageArea::GetItem(const std::string& key) {
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

  base::optional<std::string> old_value = GetItem(key);
  // TODO: Implement quota handling.
  size_bytes_ +=
      static_cast<int>(value.length() - old_value.value_or("").length());
  (*storage_map_)[key] = value;
  storage_node_->DispatchEvent(key, old_value, value);
  if (db_interface_) {
    db_interface_->Write(identifier_, key, value);
  }
}

void StorageArea::RemoveItem(const std::string& key) {
  Init();

  base::optional<std::string> old_value;
  StorageMap::iterator it = storage_map_->find(key);
  if (it != storage_map_->end()) {
    size_bytes_ -= static_cast<int>(it->second.length());
    old_value = it->second;
    storage_map_->erase(it);
  }
  storage_node_->DispatchEvent(key, old_value, base::nullopt);
  if (db_interface_) {
    db_interface_->Delete(identifier_, key);
  }
}

void StorageArea::Clear() {
  Init();

  storage_map_->clear();
  size_bytes_ = 0;
  storage_node_->DispatchEvent(base::nullopt, base::nullopt, base::nullopt);
  if (db_interface_) {
    db_interface_->Clear(identifier_);
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

  identifier_ = OriginToDatabaseIdentifier(storage_node_->origin());

  if (db_interface_) {
    // LocalStorage path. We read our StorageMap from the database.
    // Do a one-time read from the database for what's currently in
    // LocalStorage.
    identifier_ += kLocalStorageSuffix;
    db_interface_->ReadAll(
        identifier_,
        base::Bind(&StorageArea::OnInitComplete, base::Unretained(this)));
    read_event_.Wait();
  } else {
    // SessionStorage. Create a new, empty StorageMap.
    identifier_ += kSessionStorageSuffix;
    storage_map_.reset(new StorageMap);
  }
  initialized_ = true;
}

void StorageArea::OnInitComplete(scoped_ptr<StorageMap> data) {
  storage_map_.reset(data.release());
  read_event_.Signal();
}

// static
std::string StorageArea::GetLocalStorageIdForUrl(const GURL& url) {
  return OriginToDatabaseIdentifier(url.GetOrigin()) + kLocalStorageSuffix;
}

}  // namespace dom
}  // namespace cobalt
