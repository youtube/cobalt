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

#include "cobalt/webdriver/protocol/rect.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
const char kXKey[] = "x";
const char kYKey[] = "y";
const char kWidthKey[] = "width";
const char kHeightKey[] = "height";
}  // namespace

std::unique_ptr<base::Value> Rect::ToValue(const Rect& rect) {
  auto* rect_value = new base::DictionaryValue();
  rect_value->SetDouble(kXKey, rect.x_);
  rect_value->SetDouble(kYKey, rect.y_);
  rect_value->SetDouble(kWidthKey, rect.width_);
  rect_value->SetDouble(kHeightKey, rect.height_);
  return std::unique_ptr<base::Value>(rect_value);
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
