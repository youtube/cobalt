// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/webdriver/protocol/location.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
const char kXKey[] = "x";
const char kYKey[] = "y";
}  // namespace

std::unique_ptr<base::Value> Location::ToValue(const Location& location) {
  auto* location_value = new base::DictionaryValue();
  location_value->SetDouble(kXKey, location.x_);
  location_value->SetDouble(kYKey, location.y_);
  return std::unique_ptr<base::Value>(location_value);
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
