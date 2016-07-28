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

#ifndef COBALT_DOM_LOCAL_STORAGE_DATABASE_H_
#define COBALT_DOM_LOCAL_STORAGE_DATABASE_H_

#include <string>

#include "base/callback.h"
#include "cobalt/dom/storage_area.h"

namespace cobalt {
namespace storage {
class StorageManager;
}

namespace dom {

// Interacts with the StorageManager to read and write from the LocalStorage
// table in the app's SQLite database.
// The LocalStorageTable is made up of rows with three columns:
// id, key, and value. The id is an origin identifier. Each StorageArea
// has an id generated from the Window's origin.
class LocalStorageDatabase {
 public:
  typedef base::Callback<void(scoped_ptr<StorageArea::StorageMap>)>
      ReadCompletionCallback;

  explicit LocalStorageDatabase(storage::StorageManager* storage);

  // Load the LocalStorage SQL table from the Storage Manager, and extract
  // all key/values for the given origin. Calls callback and transfers ownership
  // of the hash map.
  void ReadAll(const std::string& id, const ReadCompletionCallback& callback);

  void Write(const std::string& id, const std::string& key,
             const std::string& value);
  void Delete(const std::string& id, const std::string& key);
  void Clear(const std::string& id);
  void Flush(const base::Closure& callback);

 private:
  void Init();

  storage::StorageManager* storage_;
  bool initialized_;
};
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_LOCAL_STORAGE_DATABASE_H_
