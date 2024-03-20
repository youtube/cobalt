// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/webdriver/protocol/button.h"

#include <memory>

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
const char kButtonKey[] = "button";
}

std::unique_ptr<base::Value> Button::ToValue(const Button& button) {
  base::Value ret(base::Value::Type::DICT);
  base::Value::Dict* button_object = ret.GetIfDict();
  button_object->Set(kButtonKey, button.button_);
  return base::Value::ToUniquePtrValue(std::move(ret));
}

base::Optional<Button> Button::FromValue(const base::Value* value) {
  const base::Value::Dict* dictionary_value = value->GetIfDict();
  if (!dictionary_value) {
    return base::nullopt;
  }
  absl::optional<int> button = dictionary_value->FindInt(kButtonKey);

  return Button(button.value_or(0));
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
