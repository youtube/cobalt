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

#include <algorithm>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/script/v8c/native_promise.h"
#include "cobalt/web/environment_settings_helper.h"
#include "starboard/common/murmurhash2.h"

namespace cobalt {
namespace web {
namespace cache_utils {

v8::Local<v8::String> V8String(v8::Isolate* isolate, const std::string& s) {
  return v8::String::NewFromUtf8(isolate, s.c_str())
      .FromMaybe(v8::String::Empty(isolate));
}

std::string FromV8String(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  if (!value->IsString()) {
    return "";
  }
  auto v8_string = value.As<v8::String>();
  std::string result;
  FromJSValue(isolate, v8_string, script::v8c::kNoConversionFlags, nullptr,
              &result);
  return result;
}

base::Optional<v8::Local<v8::Value>> Parse(v8::Isolate* isolate,
                                           const std::string& json) {
  return Evaluate(isolate, "{ const obj = " + json + "; obj; }");
}

base::Optional<v8::Local<v8::Value>> BaseToV8(v8::Isolate* isolate,
                                              const base::Value& value) {
  auto json = std::make_unique<std::string>();
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_OMIT_BINARY_VALUES, json.get());
  return Parse(isolate, *json);
}

base::Optional<v8::Local<v8::Promise>> OptionalPromise(
    base::Optional<v8::Local<v8::Value>> value) {
  if (!value || !(*value)->IsPromise()) {
    return base::nullopt;
  }
  return value->As<v8::Promise>();
}

std::string Stringify(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  auto global = isolate->GetCurrentContext()->Global();
  Set(global, "___tempObject", value);
  auto result = Evaluate(isolate, "JSON.stringify(___tempObject);");
  Delete(global, "___tempObject");
  if (!result) {
    return "";
  }
  return FromV8String(isolate, result.value());
}

absl::optional<base::Value> Deserialize(const std::string& json) {
  if (json.empty()) {
    return base::nullopt;
  }
  return base::JSONReader::Read(json);
}

absl::optional<base::Value> V8ToBase(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value) {
  return Deserialize(Stringify(isolate, value));
}

template <typename T>
base::Optional<v8::Local<T>> ToOptional(v8::MaybeLocal<T> value) {
  if (value.IsEmpty()) {
    return base::nullopt;
  }
  return value.ToLocalChecked();
}

v8::Isolate* GetIsolate(v8::Local<v8::Value> object) {
  if (!object->IsObject()) {
    return nullptr;
  }
  return object.As<v8::Object>()->GetIsolate();
}

base::Optional<v8::Local<v8::Value>> GetInternal(v8::Local<v8::Value> object,
                                                 const std::string& path,
                                                 bool parent) {
  auto* isolate = GetIsolate(object);
  if (!isolate) {
    return base::nullopt;
  }

  base::Optional<v8::Local<v8::Value>> curr = object;
  auto context = isolate->GetCurrentContext();
  auto parts = base::SplitString(path, ".", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  int offset = parent ? -1 : 0;
  for (int i = 0; i < parts.size() + offset; i++) {
    if (!curr || !curr.value()->IsObject()) {
      return base::nullopt;
    }
    std::string part = parts[i];
    if (part.find_first_not_of("0123456789") == std::string::npos) {
      size_t idx;
      uint32_t index = std::stoi(part, &idx);
      if (part.size() != idx) {
        return base::nullopt;
      }
      curr = ToOptional(curr->As<v8::Object>()->Get(context, index));
    } else {
      curr = ToOptional(
          curr->As<v8::Object>()->Get(context, V8String(isolate, part)));
    }
  }
  return curr;
}

base::Optional<v8::Local<v8::Value>> Get(v8::Local<v8::Value> object,
                                         const std::string& path) {
  return GetInternal(object, path, /*parent=*/false);
}

const base::Value* Get(const base::Value& value, const std::string& path,
                       bool parent) {
  if (!value.is_dict() && !value.is_list()) {
    return nullptr;
  }
  const base::Value* curr = &value;
  auto parts = base::SplitString(path, ".", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  int offset = parent ? -1 : 0;
  for (int i = 0; i < parts.size() + offset; i++) {
    std::string part = parts[i];
    if (curr->is_list()) {
      size_t idx;
      uint32_t index = std::stoi(part, &idx);
      if (part.size() != idx) {
        return nullptr;
      }
      if (index > curr->GetList().size() - 1) {
        return nullptr;
      }
      curr = &curr->GetList()[index];
    } else if (curr->is_dict()) {
      curr = curr->GetDict().Find(part);
    } else {
      return nullptr;
    }
  }
  return curr;
}

template <typename T>
using V8Transform =
    std::function<base::Optional<T>(v8::Isolate*, v8::Local<v8::Value>)>;

struct V8Transforms {
  static base::Optional<double> ToDouble(v8::Isolate* isolate,
                                         v8::Local<v8::Value> value) {
    if (!value->IsNumber()) {
      return base::nullopt;
    }
    return value.As<v8::Number>()->Value();
  }

  static base::Optional<std::string> ToString(v8::Isolate* isolate,
                                              v8::Local<v8::Value> value) {
    if (!value->IsString()) {
      return base::nullopt;
    }
    auto v8_string = value.As<v8::String>();
    std::string result;
    FromJSValue(isolate, v8_string, script::v8c::kNoConversionFlags, nullptr,
                &result);
    return std::move(result);
  }

};  // V8Transforms

template <typename T>
base::Optional<T> Get(v8::Local<v8::Value> object, const std::string& path,
                      V8Transform<T> transform) {
  auto value = GetInternal(object, path, /*parent=*/false);
  if (!value) {
    return base::nullopt;
  }
  return transform(GetIsolate(object), value.value());
}

base::Optional<std::string> GetString(v8::Local<v8::Value> object,
                                      const std::string& path) {
  return Get<std::string>(object, path, V8Transforms::ToString);
}

base::Optional<double> GetNumber(v8::Local<v8::Value> object,
                                 const std::string& path) {
  return Get<double>(object, path, V8Transforms::ToDouble);
}

bool Set(v8::Local<v8::Object> object, const std::string& key,
         v8::Local<v8::Value> value) {
  auto* isolate = object->GetIsolate();
  auto context = isolate->GetCurrentContext();
  auto result = object->Set(context, V8String(isolate, key), value);
  return result.FromMaybe(false);
}

bool Delete(v8::Local<v8::Object> object, const std::string& key) {
  auto* isolate = object->GetIsolate();
  auto context = isolate->GetCurrentContext();
  auto result = object->Delete(context, V8String(isolate, key));
  return result.FromMaybe(false);
}

double FromNumber(v8::Local<v8::Value> value) {
  return value.As<v8::Number>()->Value();
}

v8::Local<v8::Number> ToNumber(v8::Isolate* isolate, double d) {
  return v8::Number::New(isolate, d);
}

std::vector<uint8_t> ToUint8Vector(v8::Local<v8::Value> buffer) {
  if (!buffer->IsArrayBuffer()) {
    return std::vector<uint8_t>();
  }
  auto array_buffer = buffer.As<v8::ArrayBuffer>();
  auto byte_length = array_buffer->ByteLength();
  auto uint8_array =
      v8::Uint8Array::New(array_buffer, /*byte_offset=*/0, byte_length);
  auto vector = std::vector<uint8_t>(byte_length);
  uint8_array->CopyContents(vector.data(), byte_length);
  return std::move(vector);
}

base::Optional<v8::Local<v8::Value>> Call(
    v8::Local<v8::Value> object, const std::string& path,
    std::initializer_list<v8::Local<v8::Value>> args) {
  if (!object->IsObject()) {
    return base::nullopt;
  }
  v8::Local<v8::Value> result;
  auto optional_function = cache_utils::Get(object, path);
  if (!optional_function || !optional_function.value()->IsFunction()) {
    return base::nullopt;
  }
  auto context_object =
      cache_utils::GetInternal(object, path, /*parent=*/true).value();
  auto context =
      context_object.As<v8::Object>()->GetIsolate()->GetCurrentContext();
  const size_t argc = args.size();
  std::vector<v8::Local<v8::Value>> argv = args;
  return ToOptional(optional_function->As<v8::Function>()->Call(
      context, context_object, argv.size(), argv.data()));
}

base::Optional<v8::Local<v8::Value>> Then(v8::Local<v8::Value> value,
                                          OnFullfilled on_fullfilled) {
  if (!value->IsPromise()) {
    on_fullfilled.Reset();
    return base::nullopt;
  }
  auto promise = value.As<v8::Promise>();
  auto* isolate = promise->GetIsolate();
  auto context = isolate->GetCurrentContext();
  auto data = v8::Object::New(isolate);
  Set(data, "promise", promise);
  auto* on_fullfilled_ptr = new OnFullfilled(std::move(on_fullfilled));
  Set(data, "onFullfilled", v8::External::New(isolate, on_fullfilled_ptr));
  auto resulting_promise = promise->Then(
      context,
      v8::Function::New(
          context,
          [](const v8::FunctionCallbackInfo<v8::Value>& info) {
            auto promise = Get(info.Data(), "promise")->As<v8::Promise>();
            auto on_fullfilled = std::unique_ptr<OnFullfilled>(
                static_cast<OnFullfilled*>(Get(info.Data(), "onFullfilled")
                                               ->As<v8::External>()
                                               ->Value()));
            auto optional_resulting_promise =
                std::move(*on_fullfilled).Run(promise);
            if (!optional_resulting_promise) {
              return;
            }
            info.GetReturnValue().Set(optional_resulting_promise.value());
          },
          data)
          .ToLocalChecked(),
      v8::Function::New(
          context,
          [](const v8::FunctionCallbackInfo<v8::Value>& info) {
            auto on_fullfilled = std::unique_ptr<OnFullfilled>(
                static_cast<OnFullfilled*>(Get(info.Data(), "onFullfilled")
                                               ->As<v8::External>()
                                               ->Value()));
            on_fullfilled->Reset();
            info.GetIsolate()->ThrowException(info[0]);
          },
          data)
          .ToLocalChecked());
  if (resulting_promise.IsEmpty()) {
    delete on_fullfilled_ptr;
    return base::nullopt;
  }
  return resulting_promise.ToLocalChecked();
}

void Resolve(v8::Local<v8::Promise::Resolver> resolver,
             v8::Local<v8::Value> value) {
  auto* isolate = resolver->GetIsolate();
  script::v8c::EntryScope entry_scope(isolate);
  auto context = isolate->GetCurrentContext();
  if (value.IsEmpty()) {
    value = v8::Undefined(isolate);
  }
  auto result = resolver->Resolve(context, value);
  DCHECK(result.FromJust());
}

void Reject(v8::Local<v8::Promise::Resolver> resolver) {
  auto* isolate = resolver->GetIsolate();
  script::v8c::EntryScope entry_scope(isolate);
  auto context = isolate->GetCurrentContext();
  auto result = resolver->Reject(context, v8::Undefined(isolate));
  DCHECK(result.FromJust());
}

script::HandlePromiseAny FromResolver(
    v8::Local<v8::Promise::Resolver> resolver) {
  auto* isolate = resolver->GetIsolate();
  return script::HandlePromiseAny(
      new script::v8c::V8cUserObjectHolder<
          script::v8c::NativePromise<script::Any>>(isolate, resolver));
}

void Trace(v8::Isolate* isolate,
           std::initializer_list<v8::Local<v8::Value>> values,
           std::vector<v8::TracedGlobal<v8::Value>*>& traced_globals_out,
           base::OnceClosure& cleanup_traced) {
  auto heap_tracer =
      script::v8c::V8cEngine::GetFromIsolate(isolate)->heap_tracer();
  std::vector<std::unique_ptr<v8::TracedGlobal<v8::Value>>> traced_globals;
  for (auto value : values) {
    auto traced_global =
        std::make_unique<v8::TracedGlobal<v8::Value>>(isolate, value);
    heap_tracer->AddRoot(traced_global.get());
    traced_globals_out.push_back(traced_global.get());
    traced_globals.push_back(std::move(traced_global));
  }
  cleanup_traced = base::BindOnce(
      [](script::v8c::V8cHeapTracer* heap_tracer,
         std::vector<std::unique_ptr<v8::TracedGlobal<v8::Value>>>
             traced_globals) {
        for (int i = 0; i < traced_globals.size(); i++) {
          heap_tracer->RemoveRoot(traced_globals[i].get());
        }
      },
      heap_tracer, std::move(traced_globals));
}

script::Any FromV8Value(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return script::Any(new script::v8c::V8cValueHandleHolder(isolate, value));
}

v8::Local<v8::Value> ToV8Value(const script::Any& any) {
  return script::GetV8Value(*any.GetScriptValue());
}

base::Optional<v8::Local<v8::Value>> Evaluate(v8::Isolate* isolate,
                                              const std::string& js_code) {
  auto context = isolate->GetCurrentContext();
  auto script = v8::Script::Compile(context, V8String(isolate, js_code));
  if (script.IsEmpty()) {
    return base::nullopt;
  }
  return ToOptional(script.ToLocalChecked()->Run(context));
}

base::Optional<v8::Local<v8::Value>> CreateInstance(
    v8::Isolate* isolate, const std::string& class_name,
    std::initializer_list<v8::Local<v8::Value>> args) {
  auto constructor = Evaluate(isolate, class_name);
  if (!constructor) {
    return base::nullopt;
  }
  auto context = isolate->GetCurrentContext();
  std::vector<v8::Local<v8::Value>> argv = args;
  return ToOptional(constructor.value().As<v8::Function>()->NewInstance(
      context, argv.size(), argv.data()));
}

base::Optional<v8::Local<v8::Value>> CreateRequest(v8::Isolate* isolate,
                                                   const std::string& url,
                                                   const base::Value& options) {
  auto v8_options = v8::Object::New(isolate);
  auto mode = options.GetDict().Find("mode");
  if (mode) {
    Set(v8_options, "mode", V8String(isolate, mode->GetString()));
  }
  auto headers = options.GetDict().Find("headers");
  if (headers) {
    auto v8_headers = v8::Object::New(isolate);
    for (const auto& header : headers->GetList()) {
      const auto& pair = header.GetList();
      DCHECK(pair.size() == 2);
      auto name = pair[0].GetString();
      auto value = pair[1].GetString();
      Set(v8_headers, name, V8String(isolate, value));
    }
    Set(v8_options, "headers", v8_headers);
  }
  return CreateInstance(isolate, "Request",
                        {V8String(isolate, url), v8_options});
}

base::Optional<v8::Local<v8::Value>> CreateResponse(
    v8::Isolate* isolate, const std::vector<uint8_t>& body,
    const base::Value& options) {
  auto status = options.GetDict().Find("status");
  auto status_text = options.GetDict().Find("statusText");
  auto headers = options.GetDict().Find("headers");
  if (body.size() == 0 || !status || !status_text || !headers) {
    return base::nullopt;
  }
  auto v8_body = v8::ArrayBuffer::New(isolate, body.size());
  memcpy(v8_body->GetBackingStore()->Data(), body.data(), body.size());
  auto v8_options = v8::Object::New(isolate);
  Set(v8_options, "status", ToNumber(isolate, status->GetDouble()));
  Set(v8_options, "statusText", V8String(isolate, status_text->GetString()));
  auto v8_headers = v8::Object::New(isolate);
  for (const auto& header : headers->GetList()) {
    const auto& pair = header.GetList();
    DCHECK(pair.size() == 2);
    auto name = pair[0].GetString();
    auto value = pair[1].GetString();
    Set(v8_headers, name, V8String(isolate, value));
  }
  Set(v8_options, "headers", v8_headers);
  return CreateInstance(isolate, "Response", {v8_body, v8_options});
}

absl::optional<base::Value> ExtractResponseOptions(
    v8::Local<v8::Value> response) {
  if (!response->IsObject()) {
    return base::nullopt;
  }
  auto response_object = response.As<v8::Object>();
  auto* isolate = response_object->GetIsolate();
  auto context = isolate->GetCurrentContext();
  auto global = context->Global();
  Set(global, "___tempResponseObject", response_object);
  auto result = Evaluate(isolate,
                         "(() =>"
                         "___tempResponseObject instanceof Response && {"
                         "status: ___tempResponseObject.status,"
                         "statusText: ___tempResponseObject.statusText,"
                         "headers: Array.from(___tempResponseObject.headers),"
                         "}"
                         ")()");
  Delete(global, "___tempResponseObject");
  if (!result) {
    return base::nullopt;
  }
  return V8ToBase(isolate, result.value());
}

uint32_t GetKey(const std::string& s) {
  return starboard::MurmurHash2_32(s.c_str(), s.size());
}

uint32_t GetKey(const GURL& base_url,
                const script::ValueHandleHolder& request_info) {
  return GetKey(GetUrl(base_url, request_info));
}

std::string GetUrl(const GURL& base_url,
                   const script::ValueHandleHolder& request_info) {
  auto v8_value = GetV8Value(request_info);
  auto* isolate = GetIsolate(request_info);
  std::string url;
  if (v8_value->IsString()) {
    url = FromV8String(isolate, v8_value);
  } else {
    // Treat like |Request| and get "url" property.
    auto v8_url = Get(v8_value, "url");
    if (!v8_url) {
      return "";
    }
    url = FromV8String(isolate, v8_url.value());
  }
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  return base_url.Resolve(url).ReplaceComponents(replacements).spec();
}

}  // namespace cache_utils
}  // namespace web
}  // namespace cobalt
