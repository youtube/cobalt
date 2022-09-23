// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEB_CACHE_UTILS_H_
#define COBALT_WEB_CACHE_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/value_handle.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace web {
namespace cache_utils {

v8::Local<v8::String> V8String(v8::Isolate* isolate, const std::string& s);

v8::MaybeLocal<v8::Value> TryGet(v8::Local<v8::Context> context,
                                 v8::Local<v8::Value> object,
                                 const std::string& key);

v8::Local<v8::Value> Get(v8::Local<v8::Context> context,
                         v8::Local<v8::Value> object, const std::string& key);

bool Set(v8::Local<v8::Context> context, v8::Local<v8::Value> object,
         const std::string& key, v8::Local<v8::Value> value);

script::Any GetUndefined(script::EnvironmentSettings* environment_settings);

script::Any EvaluateString(script::EnvironmentSettings* environment_settings,
                           const std::string& js_code);

base::Optional<script::Any> CreateInstance(
    script::EnvironmentSettings* environment_settings,
    const std::string& class_name, int argc, v8::Local<v8::Value> argv[]);
base::Optional<script::Any> CreateRequest(
    script::EnvironmentSettings* environment_settings, const std::string& url);
base::Optional<script::Any> CreateResponse(
    script::EnvironmentSettings* environment_settings,
    std::unique_ptr<std::vector<uint8_t>> data);

uint32_t GetKey(const std::string& url);

uint32_t GetKey(script::EnvironmentSettings* environment_settings,
                const script::ValueHandleHolder& request_info);

std::string GetUrl(script::EnvironmentSettings* environment_settings,
                   const script::ValueHandleHolder& request_info);

}  // namespace cache_utils
}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_CACHE_UTILS_H_
