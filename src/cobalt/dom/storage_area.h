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

#ifndef COBALT_DOM_STORAGE_AREA_H_
#define COBALT_DOM_STORAGE_AREA_H_

#include <string>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {
class LocalStorageDatabase;
class Storage;

// StorageArea objects will be constructed and owned by dom::Storage.
// Maintains a hash table of key/value data for a given origin, as specified
// by the Storage node.
class StorageArea {
 public:
  typedef base::hash_map<std::string, std::string> StorageMap;

  // storage_node is the DOM node that owns this storage area.
  // db_interface is either NULL for SessionStorage, or a pointer to a
  // LocalStorageDatabase for LocalStorage.
  StorageArea(Storage* storage_node, LocalStorageDatabase* db_interface);
  ~StorageArea();

  int length();
  base::optional<std::string> Key(int index);
  base::optional<std::string> GetItem(const std::string& key);
  void SetItem(const std::string& key, const std::string& value);
  void RemoveItem(const std::string& key);
  void Clear();

  bool key_exists(const std::string& key_name);
  int size_bytes() const { return size_bytes_; }
  Storage* storage_node() const { return storage_node_; }
  const std::string& identifier() const { return identifier_; }

  static std::string GetLocalStorageIdForUrl(const GURL& url);

 private:
  void Init();
  void OnInitComplete(scoped_ptr<StorageMap> data);

  scoped_ptr<StorageMap> storage_map_;

  std::string identifier_;
  base::WaitableEvent read_event_;
  Storage* storage_node_;
  int size_bytes_;
  LocalStorageDatabase* db_interface_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(StorageArea);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_STORAGE_AREA_H_
