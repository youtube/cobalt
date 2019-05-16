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

#ifndef COBALT_DEBUG_JSON_OBJECT_H_
#define COBALT_DEBUG_JSON_OBJECT_H_

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace cobalt {
namespace debug {

typedef std::unique_ptr<base::DictionaryValue> JSONObject;

typedef std::unique_ptr<base::ListValue> JSONList;

JSONObject JSONParse(const std::string& json, int* parse_error);

JSONObject JSONParse(const std::string& json);

std::string JSONStringify(const JSONObject& json_object);

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_JSON_OBJECT_H_
