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

#include "cobalt/webdriver/protocol/size.h"

#include <memory>

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
const char kWidthKey[] = "width";
const char kHeightKey[] = "height";
}  // namespace

std::unique_ptr<base::Value> Size::ToValue(const Size& size) {
  base::Value ret(base::Value::Type::DICT);
  base::Value::Dict* size_value = ret.GetIfDict();
  size_value->Set(kWidthKey, size.width_);
  size_value->Set(kHeightKey, size.height_);
  return base::Value::ToUniquePtrValue(std::move(ret));
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
