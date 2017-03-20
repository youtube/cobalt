// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_c_val_key_list.h"

namespace cobalt {
namespace h5vcc {

H5vccCValKeyList::H5vccCValKeyList() {}

base::optional<std::string> H5vccCValKeyList::Item(uint32 item) {
  if (item < keys_.size()) {
    return keys_[item];
  }
  return base::nullopt;
}

uint32 H5vccCValKeyList::length() { return static_cast<uint32>(keys_.size()); }

void H5vccCValKeyList::AppendKey(const std::string& key) {
  keys_.push_back(key);
}

}  // namespace h5vcc
}  // namespace cobalt
