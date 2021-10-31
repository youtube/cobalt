// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/webdriver/protocol/frame_id.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
const char kFrameIdKey[] = "id";
}  // namespace

base::Optional<FrameId> FrameId::FromValue(const base::Value* value) {
  const base::DictionaryValue* dictionary_value;
  const base::Value* frame_id_value;
  if (value->GetAsDictionary(&dictionary_value) &&
      dictionary_value->Get(kFrameIdKey, &frame_id_value)) {
    switch (frame_id_value->type()) {
      case base::Value::Type::NONE:
        // NULL indicates the top-level browsing context.
        return FrameId(true);
      case base::Value::Type::DICTIONARY:
      case base::Value::Type::INTEGER:
      case base::Value::Type::STRING:
        // Well-formed parameter, but frames are not supported by Cobalt so
        // this will end up being NoSuchFrame error.
        return FrameId(false);
      case base::Value::Type::BINARY:
      case base::Value::Type::BOOLEAN:
      case base::Value::Type::DOUBLE:
      case base::Value::Type::LIST:
        // Malformed parameter.
        return base::nullopt;
    }
  }
  return base::nullopt;
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
