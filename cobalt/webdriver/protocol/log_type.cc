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

#include "cobalt/webdriver/protocol/log_type.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

namespace {
const char kLogTypeKey[] = "type";
}


base::Optional<LogType> LogType::FromValue(const base::Value* value) {
  const base::Value::Dict* dictionary_value = value->GetIfDict();
  if (!dictionary_value) {
    return absl::nullopt;
  }
  auto type = dictionary_value->FindString(kLogTypeKey);
  if (!type) {
    return absl::nullopt;
  }
  return LogType(*type);
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
