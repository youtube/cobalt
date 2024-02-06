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

#include "cobalt/webdriver/protocol/script.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
const char kScriptKey[] = "script";
const char kArgsKey[] = "args";
}  // namespace

base::Optional<Script> Script::FromValue(const base::Value* value) {
  const base::Value::Dict* dictionary_value = value->GetIfDict();
  if (!dictionary_value) {
    return absl::nullopt;
  }
  const std::string* function_body = dictionary_value->FindString(kScriptKey);
  if (!function_body) {
    return absl::nullopt;
  }

  // Arguments are supposed to be an array where each element in the array is
  // an argument.
  // Rather than extract the arguments here, just serialize it to a string. The
  // script execution harness will extract and convert the arguments.
  const base::Value* arguments_value = dictionary_value->Find(kArgsKey);
  if (!arguments_value) {
    return absl::nullopt;
  }

  // Ensure this is a JSON list.
  if (!arguments_value->is_list()) {
    return absl::nullopt;
  }

  std::string arguments;
  base::JSONWriter::Write(*arguments_value, &arguments);
  return Script(*function_body, arguments);
}

std::unique_ptr<base::Value> ScriptResult::ToValue(
    const ScriptResult& script_result) {
#ifndef USE_HACKY_COBALT_CHANGES
  return base::JSONReader::Read(script_result.result_string_);
#else
  return nullptr;
#endif
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
