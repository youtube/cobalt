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

#include "cobalt/web/cache_storage.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "cobalt/base/source_location.h"
#include "cobalt/cache/cache.h"
#include "cobalt/script/source_code.h"
#include "cobalt/script/v8c/v8c_value_handle.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings_helper.h"

namespace cobalt {
namespace web {

script::Handle<script::Promise<script::Handle<script::ValueHandle>>>
CacheStorage::Match(script::EnvironmentSettings* environment_settings,
                    const script::ValueHandleHolder& request) {
  return GetOrCreateCache()->Match(environment_settings, request);
}

script::Handle<script::Promise<script::Handle<script::ValueHandle>>>
CacheStorage::Match(script::EnvironmentSettings* environment_settings,
                    const script::ValueHandleHolder& request,
                    const MultiCacheQueryOptions& options) {
  return Match(environment_settings, request);
}

void CacheStorage::PerformOpen(
    std::unique_ptr<script::ValuePromiseWrappable::Reference>
        promise_reference) {
  promise_reference->value().Resolve(GetOrCreateCache());
}

script::HandlePromiseWrappable CacheStorage::Open(
    script::EnvironmentSettings* environment_settings,
    const std::string& cache_name) {
  auto* global_wrappable = get_global_wrappable(environment_settings);
  script::HandlePromiseWrappable promise =
      get_script_value_factory(environment_settings)
          ->CreateInterfacePromise<scoped_refptr<Cache>>();
  auto* context = get_context(environment_settings);
  context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CacheStorage::PerformOpen, base::Unretained(this),
                     std::make_unique<script::ValuePromiseWrappable::Reference>(
                         global_wrappable, promise)));
  return promise;
}

script::HandlePromiseBool CacheStorage::Delete(
    script::EnvironmentSettings* environment_settings,
    const std::string& cache_name) {
  auto* global_wrappable = get_global_wrappable(environment_settings);
  script::HandlePromiseBool promise =
      get_script_value_factory(environment_settings)
          ->CreateBasicPromise<bool>();
  auto promise_reference =
      std::make_unique<script::ValuePromiseBool::Reference>(global_wrappable,
                                                            promise);
  auto* context = get_context(environment_settings);
  context->message_loop()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<script::ValuePromiseBool::Reference>
                            promise_reference) {
                       cache::Cache::GetInstance()->Delete(
                           network::disk_cache::ResourceType::kCacheApi);
                       promise_reference->value().Resolve(true);
                     },
                     std::move(promise_reference)));
  return promise;
}

script::HandlePromiseBool CacheStorage::Has(
    script::EnvironmentSettings* environment_settings,
    const std::string& cache_name) {
  script::HandlePromiseBool promise =
      get_script_value_factory(environment_settings)
          ->CreateBasicPromise<bool>();
  promise->Resolve(true);
  return promise;
}

script::Handle<script::Promise<script::Handle<script::ValueHandle>>>
CacheStorage::Keys(script::EnvironmentSettings* environment_settings) {
  script::HandlePromiseAny promise =
      get_script_value_factory(environment_settings)
          ->CreateBasicPromise<script::Any>();
  auto global_environment = get_global_environment(environment_settings);
  auto* isolate = global_environment->isolate();
  promise->Resolve(script::Any(
      new script::v8c::V8cValueHandleHolder(isolate, v8::Array::New(isolate))));
  return promise;
}

scoped_refptr<Cache> CacheStorage::GetOrCreateCache() {
  if (!cache_) {
    cache_ = new Cache();
  }
  return cache_;
}

}  // namespace web
}  // namespace cobalt
