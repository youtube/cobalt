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

#include <memory>

#include "cobalt/webdriver/protocol/script.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"

namespace cobalt {
namespace webdriver {
namespace protocol {
namespace {
const char kScriptKey[] = "script";
const char kArgsKey[] = "args";
}

base::Optional<Script> Script::FromValue(const base::Value* value) {
  const base::DictionaryValue* dictionary_value;
  if (!value->GetAsDictionary(&dictionary_value)) {
    return base::nullopt;
  }
  std::string function_body;
  if (!dictionary_value->GetString(kScriptKey, &function_body)) {
    return base::nullopt;
  }

  // Arguments are supposed to be an array where each element in the array is
  // an argument.
  // Rather than extract the arguments here, just serialize it to a string. The
  // script execution harness will extract and convert the arguments.
  const base::Value* arguments_value;
  if (!dictionary_value->Get(kArgsKey, &arguments_value)) {
    return base::nullopt;
  }

  // Ensure this is a JSON list.
  if (!arguments_value->is_list()) {
    return base::nullopt;
  }

  std::string arguments;
  base::JSONWriter::Write(*arguments_value, &arguments);
  return Script(function_body, arguments);
}

std::unique_ptr<base::Value> ScriptResult::ToValue(
    const ScriptResult& script_result) {
  return base::JSONReader::Read(script_result.result_string_);
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
