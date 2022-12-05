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

#ifndef COBALT_WEB_CACHE_STORAGE_H_
#define COBALT_WEB_CACHE_STORAGE_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/cache.h"
#include "cobalt/web/multi_cache_query_options.h"

namespace cobalt {
namespace web {

class CacheStorage : public script::Wrappable {
 public:
  CacheStorage() = default;

  // Web API: CacheStorage
  //
  script::Handle<script::Promise<script::Handle<script::ValueHandle>>> Match(
      script::EnvironmentSettings* environment_settings,
      const script::ValueHandleHolder& request);
  script::Handle<script::Promise<script::Handle<script::ValueHandle>>> Match(
      script::EnvironmentSettings* environment_settings,
      const script::ValueHandleHolder& request,
      const MultiCacheQueryOptions& options);
  script::HandlePromiseWrappable Open(
      script::EnvironmentSettings* environment_settings,
      const std::string& cache_name);
  script::HandlePromiseBool Delete(
      script::EnvironmentSettings* environment_settings,
      const std::string& cache_name);
  script::HandlePromiseBool Has(
      script::EnvironmentSettings* environment_settings,
      const std::string& cache_name);
  script::Handle<script::Promise<script::Handle<script::ValueHandle>>> Keys(
      script::EnvironmentSettings* environment_settings);

  DEFINE_WRAPPABLE_TYPE(CacheStorage);

 private:
  scoped_refptr<Cache> GetOrCreateCache();
  void PerformOpen(std::unique_ptr<script::ValuePromiseWrappable::Reference>
                       promise_reference);

  scoped_refptr<Cache> cache_;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_CACHE_STORAGE_H_
