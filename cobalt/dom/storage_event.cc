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

#include "cobalt/dom/storage_event.h"

#include "cobalt/dom/event_names.h"
#include "cobalt/dom/storage.h"

namespace cobalt {
namespace dom {

StorageEvent::StorageEvent() : Event(EventNames::GetInstance()->storage()) {}

StorageEvent::StorageEvent(const base::optional<std::string>& key,
                           const base::optional<std::string>& old_value,
                           const base::optional<std::string>& new_value,
                           const std::string& url,
                           const scoped_refptr<Storage>& storage_area)
    : Event(dom::EventNames::GetInstance()->storage()),
      key_(key),
      old_value_(old_value),
      new_value_(new_value),
      url_(url),
      storage_area_(storage_area) {}

scoped_refptr<Storage> StorageEvent::storage_area() const {
  return storage_area_;
}

}  // namespace dom
}  // namespace cobalt
