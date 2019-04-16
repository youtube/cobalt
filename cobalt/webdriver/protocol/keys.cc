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

#include "cobalt/webdriver/protocol/keys.h"

#include "base/strings/string_util.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
const char kValueKey[] = "value";
}  // namespace

base::Optional<Keys> Keys::FromValue(const base::Value* value) {
  const base::DictionaryValue* dictionary;
  if (!value->GetAsDictionary(&dictionary)) {
    return base::nullopt;
  }

  const base::ListValue* list;
  if (!dictionary->GetList(kValueKey, &list)) {
    return base::nullopt;
  }

  // Each item in the list should be a string which should be flattened into
  // a single sequence of keys.
  std::string keys;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string item;
    if (!list->GetString(i, &item)) {
      return base::nullopt;
    }
    keys += item;
  }

  if (!base::IsStringUTF8(keys)) {
    return base::nullopt;
  }

  return Keys(keys);
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
