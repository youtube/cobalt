// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/memory_info.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/script/javascript_engine.h"

namespace cobalt {
namespace dom {

uint32 MemoryInfo::total_js_heap_size(
    script::EnvironmentSettings* environment_settings) const {
  if (!environment_settings) {
    return 0u;
  }
  return static_cast<uint32>(
      base::polymorphic_downcast<DOMSettings*>(environment_settings)
          ->javascript_engine()
          ->GetHeapStatistics()
          .total_heap_size);
}

uint32 MemoryInfo::used_js_heap_size(
    script::EnvironmentSettings* environment_settings) const {
  if (!environment_settings) {
    return 0u;
  }
  return static_cast<uint32>(
      base::polymorphic_downcast<DOMSettings*>(environment_settings)
          ->javascript_engine()
          ->GetHeapStatistics()
          .used_heap_size);
}

}  // namespace dom
}  // namespace cobalt
