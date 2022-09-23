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

#include "cobalt/web/cache_utils.h"

#include "cobalt/cache/cache.h"
#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/web/environment_settings_helper.h"

namespace cobalt {
namespace web {
namespace cache_utils {

v8::Local<v8::String> V8String(v8::Isolate* isolate, const std::string& s) {
  return v8::String::NewFromUtf8(isolate, s.c_str()).ToLocalChecked();
}

v8::MaybeLocal<v8::Value> TryGet(v8::Local<v8::Context> context,
                                 v8::Local<v8::Value> object,
                                 const std::string& key) {
  if (!object->IsObject()) {
    return v8::MaybeLocal<v8::Value>();
  }
  auto* isolate = context->GetIsolate();
  return object.As<v8::Object>()->Get(context, V8String(isolate, key));
}

v8::Local<v8::Value> Get(v8::Local<v8::Context> context,
                         v8::Local<v8::Value> object, const std::string& key) {
  return TryGet(context, object, key).ToLocalChecked();
}

bool Set(v8::Local<v8::Context> context, v8::Local<v8::Value> object,
         const std::string& key, v8::Local<v8::Value> value) {
  if (!object->IsObject()) {
    return false;
  }
  auto* isolate = context->GetIsolate();
  auto result =
      object.As<v8::Object>()->Set(context, V8String(isolate, key), value);
  return !result.IsNothing();
}

script::Any GetUndefined(script::EnvironmentSettings* environment_settings) {
  auto* global_environment = get_global_environment(environment_settings);
  auto* isolate = global_environment->isolate();
  return script::Any(
      new script::v8c::V8cValueHandleHolder(isolate, v8::Undefined(isolate)));
}

script::Any EvaluateString(script::EnvironmentSettings* environment_settings,
                           const std::string& js_code) {
  auto* global_environment = get_global_environment(environment_settings);
  auto* wrappable = get_global_wrappable(environment_settings);
  base::Optional<script::ValueHandleHolder::Reference> reference;
  scoped_refptr<script::SourceCode> source_code =
      script::SourceCode::CreateSourceCodeWithoutCaching(
          js_code, base::SourceLocation(__FILE__, __LINE__, 1));
  bool eval_enabled = global_environment->IsEvalEnabled();
  if (!eval_enabled) {
    global_environment->EnableEval();
  }
  bool success =
      global_environment->EvaluateScript(source_code, wrappable, &reference);
  if (!eval_enabled) {
    global_environment->DisableEval("");
  }
  if (success && reference) {
    return script::Any(reference.value());
  } else {
    return GetUndefined(environment_settings);
  }
}

base::Optional<script::Any> CreateInstance(
    script::EnvironmentSettings* environment_settings,
    const std::string& class_name, int argc, v8::Local<v8::Value> argv[]) {
  auto* global_environment = get_global_environment(environment_settings);
  auto* isolate = global_environment->isolate();
  auto reponse_function =
      cache_utils::EvaluateString(environment_settings, class_name);
  auto v8_function =
      script::GetV8Value(*reponse_function.GetScriptValue()).As<v8::Function>();
  auto context = isolate->GetCurrentContext();
  auto maybe_instance = v8_function->NewInstance(context, argc, argv);
  if (maybe_instance.IsEmpty()) {
    return base::nullopt;
  }
  return script::Any(new script::v8c::V8cValueHandleHolder(
      isolate, maybe_instance.ToLocalChecked()));
}

base::Optional<script::Any> CreateRequest(
    script::EnvironmentSettings* environment_settings, const std::string& url) {
  auto* global_environment = get_global_environment(environment_settings);
  auto* isolate = global_environment->isolate();
  v8::Local<v8::Value> argv[] = {V8String(isolate, url)};
  return CreateInstance(environment_settings, "Response", /*argc=*/1, argv);
}

base::Optional<script::Any> CreateResponse(
    script::EnvironmentSettings* environment_settings,
    std::unique_ptr<std::vector<uint8_t>> data) {
  auto* global_environment = get_global_environment(environment_settings);
  auto* isolate = global_environment->isolate();
  auto array_buffer = v8::ArrayBuffer::New(isolate, data->size());
  memcpy(array_buffer->GetBackingStore()->Data(), data->data(), data->size());
  v8::Local<v8::Value> argv[] = {array_buffer};
  return CreateInstance(environment_settings, "Response", /*argc=*/1, argv);
}

uint32_t GetKey(const std::string& url) { return cache::Cache::CreateKey(url); }

uint32_t GetKey(script::EnvironmentSettings* environment_settings,
                const script::ValueHandleHolder& request_info) {
  return GetKey(GetUrl(environment_settings, request_info));
}

std::string GetUrl(script::EnvironmentSettings* environment_settings,
                   const script::ValueHandleHolder& request_info) {
  auto v8_value = GetV8Value(request_info);
  auto* isolate = GetIsolate(request_info);
  v8::Local<v8::String> v8_string;
  if (v8_value->IsString()) {
    v8_string = v8_value.As<v8::String>();
  } else {
    auto context = isolate->GetCurrentContext();
    // Treat like |Request| and get "url" property.
    v8_string = Get(context, v8_value, "url").As<v8::String>();
  }
  std::string url;
  FromJSValue(isolate, v8_string, script::v8c::kNoConversionFlags, nullptr,
              &url);
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  return environment_settings->base_url()
      .Resolve(url)
      .ReplaceComponents(replacements)
      .spec();
}

}  // namespace cache_utils
}  // namespace web
}  // namespace cobalt
