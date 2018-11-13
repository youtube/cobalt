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
#include <string>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "cobalt/base/address_sanitizer.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "starboard/file.h"

namespace cobalt {
namespace script {
namespace v8c {

namespace {
// The life time in seconds of a snapshot data blob in memory. After this delay
// the snapshot will be deleted to save memory. The next access, if there is,
// will reload snapshot from disk.
const int kStartupDataDeleteDelay = 20;

#if !defined(COBALT_V8_BUILDTIME_SNAPSHOT)
// This file will also be touched and rebuilt every time V8 is re-built
// according to the update_snapshot_time gyp target.
const char kIsolateFellowshipBuildTime[] = __DATE__ " " __TIME__;
#endif

const char* kV8CommandLineFlags[] = {"--optimize_for_size",
                                     // Starboard disallow rwx memory access.
                                     "--write_protect_code_memory",
                                     // Cobalt's TraceMembers and
                                     // ScriptValue::*Reference do not currently
                                     // support incremental tracing.
                                     "--noincremental_marking_wrappers",
                                     // These 2 flags increase initialization
                                     // time by 1-2ms per v8 instance, but
                                     // allow deleting snapshot after startup.
                                     "--nolazy_handler_deserialization",
                                     "--nolazy_deserialization",
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

#if !defined(COBALT_V8_BUILDTIME_SNAPSHOT)
// Check build time written in the start of the snapshot file.
bool SnapshotBuildTimeMatches(const starboard::ScopedFile& snapshot_file) {
  char snapshot_time[sizeof(kIsolateFellowshipBuildTime)];
  int read =
      snapshot_file.ReadAll(snapshot_time, sizeof(kIsolateFellowshipBuildTime));
  // Logically, this could be collapsed to just "read != size", but this
  // should be read as "if the platform explicitly told us reading failed,
  // or the platform told us we read less than we expected".
  if (read == -1 || read != sizeof(kIsolateFellowshipBuildTime)) {
    LOG(ERROR) << "Reading V8 startup snapshot time failed.";
    return false;
  }

  // kIsolateFellowshipBuildTime is an auto-generated/updated time stamp when
  // v8 target is compiled to update snapshot data after any v8 change.
  if (SbMemoryCompare(snapshot_time, kIsolateFellowshipBuildTime,
                      sizeof(kIsolateFellowshipBuildTime)) != 0) {
    LOG(INFO) << "V8 code was modified since last V8 startup snapshot cache "
                 "file was generated, creating a new one.";
    return false;
  }
  return true;
}
#endif

// Remove cache file stored at the given path. The removal ability is only
// enbabled when snapshot is not generated at build-time.
void MaybeRemoveCacheFile(const std::string& file_path) {
// We should remove the existing cache file due to it being in an invalid
// state.  We will do so in the case that it either was of size 0 (but
// existed), or we failed to write it.
#if !defined(COBALT_V8_BUILDTIME_SNAPSHOT)
  if (!SbFileDelete(file_path.c_str())) {
    LOG(ERROR) << "Failed to delete V8 startup snapshot cache file.";
  }
#endif
}

// Read startup data from given file and put it in ouput_data. This function
// does not create a new v8::StartupData instance and therefore output_data
// can not be nullptr.
bool ReadStartupDataFile(const std::string& snapshot_file_path,
                         v8::StartupData* output_data) {
  starboard::ScopedFile scoped_file(snapshot_file_path.c_str(),
                                    kSbFileOpenOnly | kSbFileRead);
  if (!scoped_file.IsValid()) {
    LOG(INFO) << "Can not open file: " << snapshot_file_path;
    return false;
  }

  int64_t size = scoped_file.GetSize();
  if (size == -1) {
    LOG(ERROR) << "Received size of -1 for file that was valid: "
               << snapshot_file_path;
    return false;
  }

  if (size == 0) {
    LOG(ERROR) << "Read file of size 0: " << snapshot_file_path;
    MaybeRemoveCacheFile(snapshot_file_path);
    return false;
  }

#if !defined(COBALT_V8_BUILDTIME_SNAPSHOT)
  if (!SnapshotBuildTimeMatches(scoped_file)) {
    MaybeRemoveCacheFile(snapshot_file_path);
    return false;
  }
  size -= sizeof(kIsolateFellowshipBuildTime);
#endif
  scoped_array<char> data(new char[size]);
  int read = scoped_file.ReadAll(data.get(), size);
  if (read == -1 || read != size) {
    LOG(ERROR) << "Reading V8 startup file failed for some "
                  "unknown reason: "
               << snapshot_file_path;
    MaybeRemoveCacheFile(snapshot_file_path);
    return false;
  }
  output_data->data = data.release();
  output_data->raw_size = size;
  return true;
}

bool DeleteBlobData(v8::StartupData* data_blob) {
  if (!data_blob || !data_blob->data) {
    return false;
  }
  // Note that both us and V8 will have created this with new[], see
  // "snapshot-common.cc".  Also note that both startup data creation failure
  // from V8 is possible, and deleting a null pointer is safe, so there is no
  // DCHECK here.
  delete[] data_blob->data;
  *data_blob = {nullptr, 0};
  return true;
}
}  // namespace

class IsolateFellowship::StartupDataManager {
 public:
  StartupDataManager();
  ~StartupDataManager();

  v8::StartupData* GetSnapshotData() {
    DCHECK(snapshot_data_.data);
    return &snapshot_data_;
  }

 private:
  void LoadSnapshotBlob();

  // snapshot_data_ includes a snapshot of v8 heap that will make startup
  // faster for every v8 instance, it is either created at build time and
  // stored in content folder or created at run time and stored in the cache
  // folder depending on Cobalt version and the platform.
  v8::StartupData snapshot_data_ = {nullptr, 0};
};

IsolateFellowship::IsolateFellowship() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::IsolateFellowship");
  // TODO: Initialize V8 ICU stuff here as well.
  V8FlagsInit();
  platform = new CobaltPlatform(
      std::unique_ptr<v8::Platform>(v8::platform::CreateDefaultPlatform()));
  v8::V8::InitializePlatform(platform);
  v8::V8::Initialize();
  array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
#if defined(COBALT_V8_BUILDTIME_SNAPSHOT)
  LoadNativesBlob();
#endif
}

IsolateFellowship::~IsolateFellowship() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::~IsolateFellowship");
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();

  DCHECK(array_buffer_allocator);
  delete array_buffer_allocator;
  array_buffer_allocator = nullptr;

#if defined(COBALT_V8_BUILDTIME_SNAPSHOT)
  bool deleted = DeleteBlobData(&natives_data_);
  CHECK(deleted);
#endif
}

v8::StartupData* IsolateFellowship::GetSnapshotData() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::GetSnapshotData");
  base::AutoLock auto_lock(mutex_);
  if (!startup_data_manager_.get()) {
    startup_data_manager_.reset(new StartupDataManager());
  }
  startup_data_refcount_++;
  startup_data_last_acquired_time_ = base::TimeTicks::Now();
  return startup_data_manager_->GetSnapshotData();
}

#if defined(COBALT_V8_BUILDTIME_SNAPSHOT)
void IsolateFellowship::LoadNativesBlob() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::LoadNativesBlob");

  // Get content directory's path.
  char content_path[SB_FILE_MAX_PATH] = {};
  bool got_path = SbSystemGetPath(kSbSystemPathContentDirectory, content_path,
                                  SB_ARRAY_SIZE_INT(content_path));
  CHECK(got_path);
  std::string snapshot_file_path =
      std::string(content_path) + SB_FILE_SEP_STRING + "v8" +
      SB_FILE_SEP_STRING + V8C_INTERNAL_NATIVES_DATA_CONTENT_FILE_NAME;

  bool read = ReadStartupDataFile(snapshot_file_path, &natives_data_);
  CHECK(read);
  CHECK(natives_data_.data);
  v8::V8::SetNativesDataBlob(&natives_data_);
  LOG(INFO) << "Successfully read v8 natives blob file from content.";
}
#endif

void IsolateFellowship::ReleaseSnapshotData() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::ReleaseSnapshotData");
  base::AutoLock auto_lock(mutex_);
  startup_data_refcount_--;
  auto* current_message_loop = MessageLoop::current();
  if (current_message_loop) {
    current_message_loop->PostDelayedTask(
        FROM_HERE,
        base::Bind(&IsolateFellowship::DeleteSnapshotDataCallback,
                   base::Unretained(this)),
        base::TimeDelta::FromSeconds(kStartupDataDeleteDelay));
  }
}

// startup_data is needed for creating v8::isolate and v8::context. Cobalt
// only has one context per v8 instance. Ensure each isolate's context is
// created before deleting the startup_data, otherwise, v8 might crash.
void IsolateFellowship::DeleteSnapshotDataCallback() {
  TRACE_EVENT0(
      "cobalt::script",
      "IsolateFellowship::StartupDataManager::DeleteSnapshotDataCallback");
  base::AutoLock auto_lock(mutex_);
  // The data was accessed while this callback was pending on the message loop,
  // post it again and execute later.
  if (base::TimeTicks::Now() - startup_data_last_acquired_time_ <
      base::TimeDelta::FromSeconds(kStartupDataDeleteDelay)) {
    // If an engine was created more than kStartupDataDeleteDelay but context
    // has not been created, something must be seriously wrong.
    DCHECK(!startup_data_refcount_);
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&IsolateFellowship::DeleteSnapshotDataCallback,
                   base::Unretained(this)),
        base::TimeDelta::FromSeconds(kStartupDataDeleteDelay));
    return;
  }
  if (startup_data_manager_.get()) {
    startup_data_manager_.reset();
    LOG(INFO) << "V8 startup snapshot data blob deleted";
  }
}

IsolateFellowship::StartupDataManager::StartupDataManager() {
  TRACE_EVENT0("cobalt::script",
               "IsolateFellowship::StartupDataManager::StartupDataManager");
  LoadSnapshotBlob();
}

IsolateFellowship::StartupDataManager::~StartupDataManager() {
  TRACE_EVENT0("cobalt::script",
               "IsolateFellowship::StartupDataManager::~StartupDataManager");
  bool deleted = DeleteBlobData(&snapshot_data_);
  CHECK(deleted);
}

void IsolateFellowship::StartupDataManager::LoadSnapshotBlob() {
  TRACE_EVENT0("cobalt::script",
               "IsolateFellowship::StartupDataManager::LoadSnapshotBlob");
  DCHECK(!snapshot_data_.data);
#if defined(COBALT_V8_BUILDTIME_SNAPSHOT)
  // Get content directory's path.
  char content_path[SB_FILE_MAX_PATH] = {};
  bool got_path = SbSystemGetPath(kSbSystemPathContentDirectory, content_path,
                                  SB_ARRAY_SIZE_INT(content_path));
  CHECK(got_path);
  std::string snapshot_file_path =
      std::string(content_path) + SB_FILE_SEP_STRING + "v8" +
      SB_FILE_SEP_STRING + V8C_INTERNAL_SNAPSHOT_DATA_CONTENT_FILE_NAME;

  bool read = ReadStartupDataFile(snapshot_file_path, &snapshot_data_);
  CHECK(read);
  CHECK(snapshot_data_.data);
  LOG(INFO) << "Successfully read v8 snapshot blob file from content.";
#else
  char cache_path[SB_FILE_MAX_PATH] = {};
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, cache_path,
                       SB_ARRAY_SIZE_INT(cache_path))) {
    // If there is no cache directory, then just save the startup data in
    // memory.
    LOG(WARNING) << "Unable to read/write V8 startup snapshot data to file.";
    snapshot_data_ = v8::V8::CreateSnapshotDataBlob();
    return;
  }

  // Attempt to read the cache file.
  std::string snapshot_file_cache_dir_path =
      std::string(cache_path) + SB_FILE_SEP_STRING +
      V8C_INTERNAL_STARTUP_DATA_CACHE_FILE_NAME;
  bool read_file =
      ReadStartupDataFile(snapshot_file_cache_dir_path, &snapshot_data_);

  // If we failed to read the file, then create the snapshot data and attempt
  // to write it.
  if (!read_file) {
    snapshot_data_ = v8::V8::CreateSnapshotDataBlob();
    if (snapshot_data_.data == nullptr) {
      // Trust the V8 API, but verify.  |raw_size| should also be 0.
      DCHECK_EQ(snapshot_data_.raw_size, 0);
      // This is technically legal w.r.t. to the API documentation, but
      // *probably* indicates a serious problem (are you hacking V8
      // internals or something?).
      LOG(WARNING) << "Failed to create V8 startup snapshot.";
      return;
    }

    starboard::ScopedFile scoped_file(snapshot_file_cache_dir_path.c_str(),
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
      MaybeRemoveCacheFile(snapshot_file_cache_dir_path);
      return;
    }

    written =
        scoped_file.WriteAll(snapshot_data_.data, snapshot_data_.raw_size);
    if (written < snapshot_data_.raw_size) {
      LOG(ERROR) << "Failed to write entire V8 startup snapshot.";
      MaybeRemoveCacheFile(snapshot_file_cache_dir_path);
      return;
    }

    LOG(INFO) << "Successfully wrote V8 startup snapshot cache file.";
  }
#endif
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
