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

#ifndef COBALT_DOM_LOCAL_STORAGE_DATABASE_H_
#define COBALT_DOM_LOCAL_STORAGE_DATABASE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "cobalt/dom/storage_area.h"
#include "cobalt/loader/origin.h"

namespace cobalt {
namespace storage {
class StorageManager;
}

namespace dom {

// Interacts with the StorageManager to read and write from the LocalStorage.
class LocalStorageDatabase {
 public:
  typedef base::Callback<void(std::unique_ptr<StorageArea::StorageMap>)>
      ReadCompletionCallback;

  explicit LocalStorageDatabase(storage::StorageManager* storage);

  // Load the LocalStorage from the Storage Manager, and extract
  // all key/values for the given origin. Calls callback and transfers ownership
  // of the hash map.
  void ReadAll(const loader::Origin& origin,
               const ReadCompletionCallback& callback);

  void Write(const loader::Origin& origin, const std::string& key,
             const std::string& value);
  void Delete(const loader::Origin& origin, const std::string& key);
  void Clear(const loader::Origin& origin);
  void Flush(const base::Closure& callback);

 private:
  void Init();

  storage::StorageManager* storage_;
  bool initialized_;
};
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_LOCAL_STORAGE_DATABASE_H_
