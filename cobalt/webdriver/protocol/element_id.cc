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

#include "cobalt/webdriver/protocol/element_id.h"

#include <memory>

namespace cobalt {
namespace webdriver {
namespace protocol {

const char ElementId::kElementKey[] = "ELEMENT";

std::unique_ptr<base::Value> ElementId::ToValue(const ElementId& element_id) {
  base::Value element_object(base::Value::Type::DICT);
  element_object.GetDict().Set(kElementKey, element_id.id_);
  return base::Value::ToUniquePtrValue(std::move(element_object));
}

base::Optional<ElementId> ElementId::FromValue(const base::Value* value) {
  const base::Value::Dict* dictionary_value = value->GetIfDict();
  if (!dictionary_value) {
    return base::nullopt;
  }
  const std::string* element_id = dictionary_value->FindString(kElementKey);
  if (!element_id) {
    return base::nullopt;
  }
  return ElementId(*element_id);
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
