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

#include "cobalt/webdriver/protocol/moveto.h"

#include <memory>
#include <string>
#include <utility>

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
const char kElementKey[] = "element";
const char kXoffsetKey[] = "xoffset";
const char kYoffsetKey[] = "yoffset";
}  // namespace

std::unique_ptr<base::Value> Moveto::ToValue(const Moveto& moveto) {
  std::unique_ptr<base::DictionaryValue> moveto_object(
      new base::DictionaryValue());
  if (moveto.element_) {
    moveto_object->Set(kElementKey,
                       std::move(ElementId::ToValue(*moveto.element_)));
  }
  if (moveto.xoffset_) {
    moveto_object->SetInteger(kXoffsetKey, *moveto.xoffset_);
  }
  if (moveto.yoffset_) {
    moveto_object->SetInteger(kYoffsetKey, *moveto.yoffset_);
  }
  return std::unique_ptr<base::Value>(moveto_object.release());
}

base::Optional<Moveto> Moveto::FromValue(const base::Value* value) {
  const base::DictionaryValue* dictionary_value = nullptr;
  if (!value->GetAsDictionary(&dictionary_value)) {
    return base::nullopt;
  }

  base::Optional<ElementId> element;
  std::string element_id;
  if (dictionary_value->GetString(kElementKey, &element_id) &&
      !element_id.empty()) {
    element = ElementId(element_id);
  } else {
    const base::Value* element_value =
        value->FindKeyOfType(kElementKey, base::Value::Type::DICTIONARY);
    if (element_value) {
      element = ElementId::FromValue(element_value);
    }
  }

  int xoffset_value = 0;
  base::Optional<int> xoffset;
  if (dictionary_value->GetInteger(kXoffsetKey, &xoffset_value)) {
    xoffset = xoffset_value;
  }

  int yoffset_value = 0;
  base::Optional<int> yoffset;
  if (dictionary_value->GetInteger(kYoffsetKey, &yoffset_value)) {
    yoffset = yoffset_value;
  }

  return Moveto(element, xoffset, yoffset);
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
