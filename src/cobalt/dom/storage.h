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

#ifndef COBALT_DOM_STORAGE_H_
#define COBALT_DOM_STORAGE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/dom/storage_area.h"
#include "cobalt/script/property_enumerator.h"
#include "cobalt/script/wrappable.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

class LocalStorageDatabase;
class Window;

class Storage : public script::Wrappable {
 public:
  enum StorageType {
    kLocalStorage,
    kSessionStorage,
  };
  Storage(Window* window, StorageType type, LocalStorageDatabase* db);

  // Web API
  // https://www.w3.org/TR/2015/CR-webstorage-20150609/#storage-0
  unsigned int length() const {
    return static_cast<unsigned int>(area_->length());
  }
  base::Optional<std::string> Key(unsigned int index) const {
    return area_->Key(static_cast<int>(index));
  }
  base::Optional<std::string> GetItem(const std::string& key) const {
    return area_->GetItem(key);
  }
  void SetItem(const std::string& key, const std::string& value) {
    area_->SetItem(key, value);
  }
  void RemoveItem(const std::string& key) { area_->RemoveItem(key); }
  void Clear() { area_->Clear(); }

  // Custom, not in any spec.
  bool CanQueryNamedProperty(const std::string& name) const;

  // Custom, not in any spec.
  void EnumerateNamedProperties(script::PropertyEnumerator* enumerator) const;

  virtual bool DispatchEvent(const base::Optional<std::string>& key,
                             const base::Optional<std::string>& old_value,
                             const base::Optional<std::string>& new_value);
  virtual GURL origin() const;

  DEFINE_WRAPPABLE_TYPE(Storage);

 protected:
  Window* window_;
  std::unique_ptr<StorageArea> area_;

  FRIEND_TEST(StorageAreaTest, InitialState);
  FRIEND_TEST(StorageAreaTest, Identifier);

 private:
  DISALLOW_COPY_AND_ASSIGN(Storage);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_STORAGE_H_
