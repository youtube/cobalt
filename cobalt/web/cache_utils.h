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

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/value_handle.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace web {
namespace cache_utils {

using OnFullfilled = base::OnceCallback<base::Optional<v8::Local<v8::Promise>>(
    v8::Local<v8::Promise>)>;

v8::Local<v8::String> V8String(v8::Isolate* isolate, const std::string& s);

std::string FromV8String(v8::Isolate* isolate, v8::Local<v8::Value> value);

base::Optional<v8::Local<v8::Promise>> OptionalPromise(
    base::Optional<v8::Local<v8::Value>> value);

const base::Value* Get(const base::Value& value, const std::string& path,
                       bool parent = false);

base::Optional<v8::Local<v8::Value>> Get(v8::Local<v8::Value> object,
                                         const std::string& path);

template <typename T>
inline T* GetExternal(v8::Local<v8::Value> object, const std::string& path) {
  base::Optional<v8::Local<v8::Value>> value = Get(object, path);
  if (!value) {
    return nullptr;
  }
  return static_cast<T*>(value->As<v8::External>()->Value());
}

template <typename T>
inline std::unique_ptr<T> GetOwnedExternal(v8::Local<v8::Object> object,
                                           const std::string& path) {
  return std::unique_ptr<T>(web::cache_utils::GetExternal<T>(object, path));
}

base::Optional<double> GetNumber(v8::Local<v8::Value> object,
                                 const std::string& path);
base::Optional<std::string> GetString(v8::Local<v8::Value> object,
                                      const std::string& path);

bool Set(v8::Local<v8::Object> object, const std::string& key,
         v8::Local<v8::Value> value);

template <typename T>
inline bool SetExternal(v8::Local<v8::Object> object, const std::string& key,
                        T* value) {
  auto* isolate = object->GetIsolate();
  return Set(object, key, v8::External::New(isolate, value));
}

template <typename T>
inline bool SetOwnedExternal(v8::Local<v8::Object> object,
                             const std::string& key, std::unique_ptr<T> value) {
  return SetExternal<T>(object, key, value.release());
}

bool Delete(v8::Local<v8::Object> object, const std::string& key);

base::Optional<v8::Local<v8::Value>> Call(
    v8::Local<v8::Value> object, const std::string& key,
    std::initializer_list<v8::Local<v8::Value>> args = {});
base::Optional<v8::Local<v8::Value>> Then(v8::Local<v8::Value> value,
                                          OnFullfilled on_fullfilled);
void Resolve(v8::Local<v8::Promise::Resolver> resolver,
             v8::Local<v8::Value> value = v8::Local<v8::Value>());
void Reject(v8::Local<v8::Promise::Resolver> resolver);
script::HandlePromiseAny FromResolver(
    v8::Local<v8::Promise::Resolver> resolver);

// Keeps |values| from being garbage collected until |cleanup_traced| is run.
// The caller needs |traced_globals_out| to get a valid reference of the
// |v8::Value|. To get a reference, the caller needs to create a
// |v8::HandleScope| and then call |v8::TracedGlobal::Get| to get a valid
// |v8::Value|.
// |values| will have a 1:1 relationship with |traced_globals_out|.
// It is up to the caller to ensure that |cleanup_traced| is run in all
// possible code paths.
void Trace(v8::Isolate* isolate,
           std::initializer_list<v8::Local<v8::Value>> values,
           std::vector<v8::TracedGlobal<v8::Value>*>& traced_globals_out,
           base::OnceClosure& cleanup_traced);

std::string Stringify(v8::Isolate* isolate, v8::Local<v8::Value> value);
base::Optional<v8::Local<v8::Value>> BaseToV8(v8::Isolate* isolate,
                                              const base::Value& value);
absl::optional<base::Value> Deserialize(const std::string& json);
absl::optional<base::Value> V8ToBase(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value);

double FromNumber(v8::Local<v8::Value> value);
v8::Local<v8::Number> ToNumber(v8::Isolate* isolate, double d);

std::vector<uint8_t> ToUint8Vector(v8::Local<v8::Value> buffer);

script::Any FromV8Value(v8::Isolate* isolate, v8::Local<v8::Value> value);
v8::Local<v8::Value> ToV8Value(const script::Any& any);

base::Optional<v8::Local<v8::Value>> Evaluate(v8::Isolate* isolate,
                                              const std::string& js_code);

base::Optional<v8::Local<v8::Value>> CreateRequest(
    v8::Isolate* isolate, const std::string& url,
    const base::Value& options = base::Value(base::Value::Type::DICT));
base::Optional<v8::Local<v8::Value>> CreateResponse(
    v8::Isolate* isolate, const std::vector<uint8_t>& body,
    const base::Value& options);
absl::optional<base::Value> ExtractResponseOptions(
    v8::Local<v8::Value> response);

uint32_t GetKey(const std::string& s);

uint32_t GetKey(const GURL& base_url,
                const script::ValueHandleHolder& request_info);

std::string GetUrl(const GURL& base_url,
                   const script::ValueHandleHolder& request_info);

}  // namespace cache_utils
}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_CACHE_UTILS_H_
