// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_ISOLATE_FELLOWSHIP_H_
#define COBALT_SCRIPT_V8C_ISOLATE_FELLOWSHIP_H_

#include "base/memory/singleton.h"
#include "v8/include/v8-platform.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// A helper singleton to hold global state required by all V8 isolates across
// the entire process (the fellowship).  Initialization logic for this data is
// also handled here, most notably, the possible reading/writing of
// |startup_data| from/to a cache file.
struct IsolateFellowship {
 public:
  static IsolateFellowship* GetInstance() {
    return Singleton<IsolateFellowship,
                     StaticMemorySingletonTraits<IsolateFellowship>>::get();
  }

  v8::Platform* platform = nullptr;
  v8::ArrayBuffer::Allocator* array_buffer_allocator = nullptr;
  v8::StartupData startup_data = {nullptr, 0};

  friend struct StaticMemorySingletonTraits<IsolateFellowship>;

 private:
  IsolateFellowship();
  ~IsolateFellowship();

  // Initialize |startup_data| by either reading it from a cache file or
  // creating it.  If creating it for the first time, then when appropriate
  // (i.e. the platform has a suitable directory) attempt to write it to a cache
  // file.
  void InitializeStartupData();
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_ISOLATE_FELLOWSHIP_H_
