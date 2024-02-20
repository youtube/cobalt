// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_MEMORY_INFO_H_
#define COBALT_DOM_MEMORY_INFO_H_

#include "cobalt/media/decoder_buffer_allocator.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// This interface contains information related to JS memory usage.
// This is a non-standard interface in Chromium.
//   https://docs.webplatform.org/wiki/apis/timing/properties/memory
class MemoryInfo : public script::Wrappable {
 public:
  MemoryInfo() = default;

  uint32 total_js_heap_size(
      script::EnvironmentSettings* environment_settings) const;
  uint32 used_js_heap_size(
      script::EnvironmentSettings* environment_settings) const;
  uint32 media_source_size_limit(
      script::EnvironmentSettings* environment_settings) const;
  uint32 total_media_source_size(
      script::EnvironmentSettings* environment_settings) const;
  uint32 used_media_source_memory_size(
      script::EnvironmentSettings* environment_settings) const;

  DEFINE_WRAPPABLE_TYPE(MemoryInfo);

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryInfo);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEMORY_INFO_H_
