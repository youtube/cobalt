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

#ifndef COBALT_DOM_STORAGE_EVENT_H_
#define COBALT_DOM_STORAGE_EVENT_H_

#include <string>

#include "base/optional.h"
#include "cobalt/dom/event.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {
class Storage;

class StorageEvent : public Event {
 public:
  StorageEvent();
  explicit StorageEvent(const std::string& type);
  StorageEvent(const base::Optional<std::string>& key,
               const base::Optional<std::string>& old_value,
               const base::Optional<std::string>& new_value,
               const std::string& url,
               const scoped_refptr<Storage>& storage_area);

  base::Optional<std::string> key() const { return key_; }
  base::Optional<std::string> old_value() const { return old_value_; }
  base::Optional<std::string> new_value() const { return new_value_; }
  std::string url() const { return url_; }
  scoped_refptr<Storage> storage_area() const;

  DEFINE_WRAPPABLE_TYPE(StorageEvent);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  base::Optional<std::string> key_;
  base::Optional<std::string> old_value_;
  base::Optional<std::string> new_value_;
  std::string url_;
  scoped_refptr<Storage> storage_area_;

  DISALLOW_COPY_AND_ASSIGN(StorageEvent);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_STORAGE_EVENT_H_
