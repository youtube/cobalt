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

#include <memory>

#include "cobalt/webdriver/protocol/button.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
const char kButtonKey[] = "button";
}

std::unique_ptr<base::Value> Button::ToValue(const Button& button) {
  std::unique_ptr<base::DictionaryValue> button_object(
      new base::DictionaryValue());
  button_object->SetInteger(kButtonKey, button.button_);
  return std::unique_ptr<base::Value>(button_object.release());
}

base::Optional<Button> Button::FromValue(const base::Value* value) {
  const base::DictionaryValue* dictionary_value;
  if (!value->GetAsDictionary(&dictionary_value)) {
    return base::nullopt;
  }
  int button = 0;
  dictionary_value->GetInteger(kButtonKey, &button);

  return Button(button);
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
