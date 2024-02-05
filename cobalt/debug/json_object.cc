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

#include "cobalt/debug/json_object.h"

#include "base/logging.h"

namespace cobalt {
namespace debug {

JSONObject JSONParse(const std::string& json, int* parse_error) {
  base::JSONReader::Result result =
      base::JSONReader::ReadAndReturnValueWithError(json);
  if (result.has_value()) {
    DCHECK(result.value().is_dict());
    return JSONObject(std::make_unique<base::Value::Dict>(
        std::move(result.value()->GetDict())));
  } else if (parse_error) {
#ifdef USE_HACKY_COBALT_CHANGES
    *parse_error = 0;
#endif
  }
  // Scoped pointer may be NULL - caller must check.
  return JSONObject(nullptr);
}

JSONObject JSONParse(const std::string& json) { return JSONParse(json, NULL); }

std::string JSONStringify(const JSONObject& json_object) {
  if (!json_object) return "{}";
  std::string json;
  base::JSONWriter::Write(*(json_object.get()), &json);
  return json;
}
}  // namespace debug
}  // namespace cobalt
