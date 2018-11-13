// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <vector>

#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/timer.h"
#include "cobalt/script/v8c/cobalt_platform.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8-platform.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// A helper singleton to hold global state required by all V8 isolates across
// the entire process (the fellowship).  Initialization logic for this data is
// also handled here, most notably, the possible reading/writing of
// |startup_data| from/to a cache file if snapshot has not been generated at
// build time.
class IsolateFellowship {
 public:
  static IsolateFellowship* GetInstance() {
    return Singleton<IsolateFellowship,
                     StaticMemorySingletonTraits<IsolateFellowship>>::get();
  }

  scoped_refptr<CobaltPlatform> platform = nullptr;
  v8::ArrayBuffer::Allocator* array_buffer_allocator = nullptr;
  // Obtain the snapshot data for the use of v8 initialization.
  v8::StartupData* GetSnapshotData();
  // Signal IsolateFellowship that current v8 JavaScript engine is done with
  // the snapshot data and IsolateFellowship can delete the snapshot data any
  // time later.
  void ReleaseSnapshotData();

  friend struct StaticMemorySingletonTraits<IsolateFellowship>;

 private:
  // A helper class to manage the lifetime of v8's snapshot and natives blob
  // data.
  class StartupDataManager;

  void DeleteSnapshotDataCallback();
  IsolateFellowship();
  ~IsolateFellowship();
#if defined(COBALT_V8_BUILDTIME_SNAPSHOT)
  void LoadNativesBlob();
#endif

#if defined(COBALT_V8_BUILDTIME_SNAPSHOT)
  // natives_data_ include some JavaScript libraries compiled into c code.
  // This is only required by build-time generated startup data. V8 will point
  // directly into this data for library references so we can't delete it
  // while v8 is running. Without build-time snapshot, the same c codes are
  // compiled into v8.
  v8::StartupData natives_data_ = {nullptr, 0};
#endif
  scoped_ptr<StartupDataManager> startup_data_manager_;
  // The number of engines that have been created but its v8 context has not.
  int startup_data_refcount_ = 0;
  base::Lock mutex_;
  base::TimeTicks startup_data_last_acquired_time_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_ISOLATE_FELLOWSHIP_H_
