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

#include "cobalt/script/v8c/isolate_fellowship.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/script/v8c/v8c_tracing_controller.h"
#include "starboard/file.h"

namespace cobalt {
namespace script {
namespace v8c {

namespace {
// This file will also be touched and rebuilt every time V8 is re-built
// according to the update_snapshot_time gyp target.
const char kIsolateFellowshipBuildTime[] = __DATE__ " " __TIME__;

const char* kV8CommandLineFlags[] = {"--optimize_for_size",
                                     // Starboard disallow rwx memory access.
                                     "--write_protect_code_memory",
                                     // Cobalt's TraceMembers and
                                     // ScriptValue::*Reference do not currently
                                     // support incremental tracing.
                                     "--noincremental_marking_wrappers",
                                     "--noexpose_wasm",
                                     "--novalidate_asm",
#if defined(COBALT_GC_ZEAL)
                                     "--gc_interval=1200"
#endif
};

// Configure v8's global command line flag options for Cobalt.
// It can be called more than once, but make sure it is called before any
// v8 instance is created.
void V8FlagsInit() {
  for (auto flag_str : kV8CommandLineFlags) {
    v8::V8::SetFlagsFromString(flag_str, SbStringGetLength(flag_str));
  }
}

}  // namespace

IsolateFellowship::IsolateFellowship() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::IsolateFellowship");
  // TODO: Initialize V8 ICU stuff here as well.
  platform.reset(new CobaltPlatform(v8::platform::NewDefaultPlatform(
      0 /*thread_pool_size*/, v8::platform::IdleTaskSupport::kDisabled,
      v8::platform::InProcessStackDumping::kEnabled,
      std::unique_ptr<v8::TracingController>(new V8cTracingController()))));
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();

  // If a new snapshot data is needed, a temporary v8 isolate will be created
  // to write the snapshot data. We need to make sure all global command line
  // flags are set before that.
  V8FlagsInit();
#if !defined(COBALT_V8_BUILDTIME_SNAPSHOT)
  InitializeStartupData();
#endif  // !defined(COBALT_V8_BUILDTIME_SNAPSHOT)
}

IsolateFellowship::~IsolateFellowship() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::~IsolateFellowship");
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();

  DCHECK(array_buffer_allocator);
  delete array_buffer_allocator;
  array_buffer_allocator = nullptr;

#if !defined(COBALT_V8_BUILDTIME_SNAPSHOT)
  // Note that both us and V8 will have created this with new[], see
  // "snapshot-common.cc".  Also note that both startup data creation failure
  // from V8 is possible, and deleting a null pointer is safe, so there is no
  // DCHECK here.
  delete[] startup_data.data;
  startup_data = {nullptr, 0};
#endif
}

#if !defined(COBALT_V8_BUILDTIME_SNAPSHOT)
void IsolateFellowship::InitializeStartupData() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::InitializeStartupData");
  DCHECK(startup_data.data == nullptr);

  std::vector<char> cache_path(SB_FILE_MAX_PATH);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, cache_path.data(),
                       cache_path.size())) {
    // If there is no cache directory, then just save the startup data in
    // memory.
    LOG(WARNING) << "Unable to read/write V8 startup snapshot data to file.";
    startup_data = v8::V8::CreateSnapshotDataBlob();
    return;
  }

  // Whether or not we should attempt to remove the existing cache file due to
  // it being in an invalid state.  We will do so in the case that it either
  // was of size 0 (but existed), or we failed to write it.
  bool should_remove_cache_file = false;

  // Attempt to read the cache file.
  std::string snapshot_file_full_path =
      std::string(cache_path.data()) + SB_FILE_SEP_STRING +
      V8C_INTERNAL_STARTUP_DATA_CACHE_FILE_NAME;
  bool read_file = ([&]() {
    starboard::ScopedFile scoped_file(snapshot_file_full_path.c_str(),
                                      kSbFileOpenOnly | kSbFileRead);
    if (!scoped_file.IsValid()) {
      LOG(INFO) << "Can not open snapshot file";
      return false;
    }

    int64_t size = scoped_file.GetSize();
    if (size == -1) {
      LOG(ERROR) << "Received size of -1 for file that was valid.";
      return false;
    }

    if (size == 0) {
      LOG(ERROR) << "Read V8 snapshot file of size 0.";
      should_remove_cache_file = true;
      return false;
    }
    int64_t data_size = size - sizeof(kIsolateFellowshipBuildTime);

    char snapshot_time[sizeof(kIsolateFellowshipBuildTime)];
    int read =
        scoped_file.ReadAll(snapshot_time, sizeof(kIsolateFellowshipBuildTime));
    // Logically, this could be collapsed to just "read != data_size", but this
    // should be read as "if the platform explicitly told us reading failed,
    // or the platform told us we read less than we expected".
    if (read == -1 || read != sizeof(kIsolateFellowshipBuildTime)) {
      LOG(ERROR) << "Reading V8 startup snapshot time failed.";
      should_remove_cache_file = true;
      return false;
    }

    // kIsolateFellowshipBuildTime is an auto-generated/updated time stamp when
    // v8 target is compiled to update snapshot data after any v8 change.
    if (SbMemoryCompare(snapshot_time, kIsolateFellowshipBuildTime,
                        sizeof(kIsolateFellowshipBuildTime)) != 0) {
      LOG(INFO) << "V8 code was modified since last V8 startup snapshot cache "
                   "file was generated, creating a new one.";
      should_remove_cache_file = true;
      return false;
    }

    std::unique_ptr<char[]> data(new char[data_size]);
    read = scoped_file.ReadAll(data.get(), data_size);
    if (read == -1 || read != data_size) {
      LOG(ERROR) << "Reading V8 startup snapshot cache file failed for some "
                    "unknown reason.";
      should_remove_cache_file = true;
      return false;
    }

    LOG(INFO) << "Successfully read V8 startup snapshot cache file.";
    startup_data.data = data.release();
    startup_data.raw_size = data_size;
    return true;
  })();

  auto maybe_remove_cache_file = [&]() {
    if (should_remove_cache_file) {
      if (!SbFileDelete(snapshot_file_full_path.c_str())) {
        LOG(ERROR) << "Failed to delete V8 startup snapshot cache file.";
      }
      should_remove_cache_file = false;
    }
  };
  maybe_remove_cache_file();

  // If we failed to read the file, then create the snapshot data and attempt
  // to write it.
  if (!read_file) {
    ([&]() {
      startup_data = v8::V8::CreateSnapshotDataBlob();
      if (startup_data.data == nullptr) {
        // Trust the V8 API, but verify.  |raw_size| should also be 0.
        DCHECK_EQ(startup_data.raw_size, 0);
        // This is technically legal w.r.t. to the API documentation, but
        // *probably* indicates a serious problem (are you hacking V8
        // internals or something?).
        LOG(WARNING) << "Failed to create V8 startup snapshot.";
        return;
      }

      starboard::ScopedFile scoped_file(snapshot_file_full_path.c_str(),
                                        kSbFileCreateOnly | kSbFileWrite);
      if (!scoped_file.IsValid()) {
        LOG(ERROR)
            << "Failed to open V8 startup snapshot cache file for writing.";
        return;
      }

      int written = scoped_file.WriteAll(kIsolateFellowshipBuildTime,
                                         sizeof(kIsolateFellowshipBuildTime));
      if (written < sizeof(kIsolateFellowshipBuildTime)) {
        LOG(ERROR) << "Failed to write V8 startup snapshot time.";
        should_remove_cache_file = true;
        return;
      }

      written = scoped_file.WriteAll(startup_data.data, startup_data.raw_size);
      if (written < startup_data.raw_size) {
        LOG(ERROR) << "Failed to write entire V8 startup snapshot.";
        should_remove_cache_file = true;
        return;
      }

      LOG(INFO) << "Successfully wrote V8 startup snapshot cache file.";
    })();
  }

  maybe_remove_cache_file();
}
#endif  // !defined(COBALT_V8_BUILDTIME_SNAPSHOT)

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
