// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/core/cobalt/memory_info/media_source_memory_info.h"

#include "media/base/decoder_buffer.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {

uint64_t MediaSourceMemoryInfo::mediaSourceSizeLimit(MemoryInfo&) {
  return Platform::Current()->mediaSourceSizeLimit();
}

uint64_t MediaSourceMemoryInfo::totalMediaSourceSize(MemoryInfo&) {
  return Platform::Current()->totalMediaSourceSize();
}

uint64_t MediaSourceMemoryInfo::usedMediaSourceMemorySize(MemoryInfo&) {
  return Platform::Current()->usedMediaSourceMemorySize();
}

}  //  namespace blink
