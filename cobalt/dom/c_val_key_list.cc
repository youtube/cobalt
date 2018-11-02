// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/c_val_key_list.h"

#include <algorithm>
#include <iterator>

namespace cobalt {
namespace dom {

CValKeyList::CValKeyList() {}

base::optional<std::string> CValKeyList::Item(uint32 item) {
  if (item < keys_.size()) {
    return keys_[item];
  }
  return base::nullopt;
}

uint32 CValKeyList::length() { return static_cast<uint32>(keys_.size()); }

int32 CValKeyList::IndexOf(const std::string& key) {
  auto iter = std::find(keys_.begin(), keys_.end(), key);
  return iter == keys_.end()
             ? -1
             : static_cast<int32>(std::distance(keys_.begin(), iter));
}

void CValKeyList::AppendKey(const std::string& key) { keys_.push_back(key); }

}  // namespace dom
}  // namespace cobalt
