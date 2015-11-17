/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/webdriver/protocol/element_id.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
const char kElementKey[] = "ELEMENT";
}

scoped_ptr<base::Value> ElementId::ToValue(const ElementId& element_id) {
  scoped_ptr<base::DictionaryValue> element_object(new base::DictionaryValue());
  element_object->SetString(kElementKey, element_id.id_);
  return element_object.PassAs<base::Value>();
}

base::optional<ElementId> ElementId::FromValue(const base::Value* value) {
  const base::DictionaryValue* dictionary_value;
  if (!value->GetAsDictionary(&dictionary_value)) {
    return base::nullopt;
  }
  std::string element_id;
  if (!dictionary_value->GetString(kElementKey, &element_id)) {
    return base::nullopt;
  }
  return ElementId(element_id);
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
