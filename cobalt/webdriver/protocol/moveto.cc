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
  base::Value ret(base::Value::Type::DICT);
  base::Value::Dict* moveto_object = ret->GetIfDict();
  if (moveto.element_) {
    moveto_object->Set(kElementKey,
                       ElementId::ToValue(*moveto.element_)->Clone());
  }
  if (moveto.xoffset_) {
    moveto_object->Set(kXoffsetKey, *moveto.xoffset_);
  }
  if (moveto.yoffset_) {
    moveto_object->Set(kYoffsetKey, *moveto.yoffset_);
  }
  return base::Value::ToUniquePtrValue(std::move(ret));
}

base::Optional<Moveto> Moveto::FromValue(const base::Value* value) {
  const base::Value::Dict* dictionary_value = value->GetIfDict();
  if (!dictionary_value) {
    return base::nullopt;
  }

  base::Optional<ElementId> element;
  const std::string* element_id = dictionary_value->FindString(kElementKey);
  if (element_id && !element_id->empty()) {
    element = ElementId(*element_id);
  } else {
    const base::Value* element_value = dictionary_value->Find(kElementKey);
    if (element_value) {
      element = ElementId::FromValue(element_value);
    }
  }

  absl::optional<int> xoffset = dictionary_value->FindInt(kXoffsetKey);
  absl::optional<int> yoffset = dictionary_value->FindInt(kYoffsetKey);

  return Moveto(element, xoffset.value_or(0), yoffset.value_or(0));
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
