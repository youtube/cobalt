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

#include "cobalt/script/v8c/isolate_fellowship.h"

#include <string>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "starboard/file.h"
#include "v8/include/libplatform/libplatform.h"

namespace cobalt {
namespace script {
namespace v8c {

IsolateFellowship::IsolateFellowship() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::IsolateFellowship");
  // TODO: Initialize V8 ICU stuff here as well.
  platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform);
  v8::V8::Initialize();
  array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();

  InitializeStartupData();
}

IsolateFellowship::~IsolateFellowship() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::~IsolateFellowship");
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();

  DCHECK(platform);
  delete platform;
  platform = nullptr;

  DCHECK(array_buffer_allocator);
  delete array_buffer_allocator;
  array_buffer_allocator = nullptr;

  // Note that both us and V8 will have created this with new[], see
  // "snapshot-common.cc".  Also note that both startup data creation failure
  // from V8 is possible, and deleting a null pointer is safe, so there is no
  // DCHECK here.
  delete[] startup_data.data;
  startup_data = {nullptr, 0};
}

void IsolateFellowship::InitializeStartupData() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::InitializeStartupData");
  DCHECK(startup_data.data == nullptr);

  char cache_path[SB_FILE_MAX_PATH] = {};
  char executable_path[SB_FILE_MAX_PATH] = {};
  SbFileInfo executable_info;
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, cache_path,
                       SB_ARRAY_SIZE_INT(cache_path)) ||
      !SbSystemGetPath(kSbSystemPathExecutableFile, executable_path,
                       SB_ARRAY_SIZE_INT(executable_path)) ||
      !SbFileGetPathInfo(executable_path, &executable_info)) {
    // If either of these conditions fail (there is no cache directory or we
    // can't stat our own executable), then just save the startup data in
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
  std::string full_path = std::string(cache_path) + SB_FILE_SEP_STRING +
                          V8C_INTERNAL_STARTUP_DATA_CACHE_FILE_NAME;
  bool read_file = ([&]() {
    starboard::ScopedFile scoped_file(full_path.c_str(),
                                      kSbFileOpenOnly | kSbFileRead);
    if (!scoped_file.IsValid()) {
      return false;
    }

    SbFileInfo cache_info;
    if (!scoped_file.GetInfo(&cache_info)) {
      LOG(WARNING)
          << "Failed to get info for cache file that we successfully opened.";
      should_remove_cache_file = true;
      return false;
    }

    if (executable_info.creation_time > cache_info.creation_time) {
      LOG(INFO) << "Running executable newer than V8 startup snapshot cache "
                   "file, creating a new one.";
      should_remove_cache_file = true;
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

    scoped_array<char> data(new char[size]);
    int read = scoped_file.ReadAll(data.get(), size);
    // Logically, this could be collapsed to just "read != size", but this
    // should be read as "if the platform explicitly told us reading failed,
    // or the platform told us we read less than we expected".
    if (read == -1 || read != size) {
      LOG(ERROR) << "Reading V8 startup snapshot cache file failed for some "
                    "unknown reason.";
      return false;
    }

    LOG(INFO) << "Successfully read V8 startup snapshot cache file.";
    startup_data.data = data.release();
    startup_data.raw_size = size;
    return true;
  })();

  auto maybe_remove_cache_file = [&]() {
    if (should_remove_cache_file) {
      if (!SbFileDelete(full_path.c_str())) {
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

      starboard::ScopedFile scoped_file(full_path.c_str(),
                                        kSbFileCreateOnly | kSbFileWrite);
      if (!scoped_file.IsValid()) {
        LOG(ERROR)
            << "Failed to open V8 startup snapshot cache file for writing.";
        return;
      }

      int written =
          scoped_file.WriteAll(startup_data.data, startup_data.raw_size);
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

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
