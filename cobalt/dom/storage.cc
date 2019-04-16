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

#include "cobalt/dom/storage.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/storage_area.h"
#include "cobalt/dom/storage_event.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace dom {

Storage::Storage(Window* window, StorageType type,
                 LocalStorageDatabase* db_interface)
    : window_(window) {
  if (type == kLocalStorage) {
    DCHECK(db_interface) << "Valid LocalStorageDatabase required";
    area_.reset(new StorageArea(this, db_interface));
  } else if (type == kSessionStorage) {
    area_.reset(new StorageArea(this, NULL));
  } else {
    NOTREACHED() << "Invalid storage type " << type;
  }
}

bool Storage::CanQueryNamedProperty(const std::string& name) const {
  return area_->key_exists(name);
}

void Storage::EnumerateNamedProperties(
    script::PropertyEnumerator* enumerator) const {
  const uint32 num_entries = length();
  for (uint32 i = 0; i < num_entries; ++i) {
    base::Optional<std::string> key = Key(i);
    if (key) {
      enumerator->AddProperty(key.value());
    }
  }
}

bool Storage::DispatchEvent(const base::Optional<std::string>& key,
                            const base::Optional<std::string>& old_value,
                            const base::Optional<std::string>& new_value) {
  return window_->DispatchEvent(new StorageEvent(
      key, old_value, new_value, window_->document()->url(), this));
}

GURL Storage::origin() const { return window_->document()->url_as_gurl(); }

}  // namespace dom
}  // namespace cobalt
