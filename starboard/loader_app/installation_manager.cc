// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/installation_manager.h"

#include <set>
#include <string>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/loader_app/installation_store.pb.h"
#if !SB_IS(EVERGREEN_COMPATIBLE_LITE)
#include "starboard/loader_app/pending_restart.h"
#endif  // !SB_IS(EVERGREEN_COMPATIBLE_LITE)
#include "starboard/once.h"
#include "starboard/string.h"

namespace starboard {
namespace loader_app {
namespace installation_manager {

class InstallationManager {
 public:
  explicit InstallationManager(int max_num_installations,
                               const std::string& app_key);

  int Initialize();
  int GetAppKey(char* app_key, int app_key_length);
  int GetMaxNumberInstallations();
  int Reset();
  int GetInstallationStatus(int installation_index);
  int GetInstallationNumTriesLeft(int installation_index);
  int RollForwardIfNeeded();
  int RollForward(int installation_index);
  int DecrementInstallationNumTries(int installation_index);

  int RevertToSuccessfulInstallation();
  int GetInstallationPath(int installation_index, char* path, int path_length);
  int GetCurrentInstallationIndex();
  int MarkInstallationSuccessful(int installation_index);
  int SelectNewInstallationIndex();
  int ResetInstallation(int installation_index);
  int RequestRollForwardToInstallation(int installation_index);

 private:
  bool IsValidIndex(int i);
  void ValidatePriorities();
  int FindCurrentInstallationIndex();
  void CreateInstallationStore();
  std::string DumpInstallationSlots();
  void LogLastSystemError(const char* msg);
  void ShiftPrioritiesInRange(int high_priority,
                              int low_priority,
                              int shift_amount);

  int RollForwardInternal(int installation);
  bool InitInstallationStorePath();
  bool LoadInstallationStore();
  bool SaveInstallationStore();
  bool CreateInstallationDirs();
  bool CleanInstallationDirs();
  bool GetInstallationPathInternal(int installation_index,
                                   char* path,
                                   int path_length);

  cobalt::loader::InstallationStore installation_store_;
  bool initialized_;
  int current_installation_;
  std::string app_key_;
  std::string store_path_;
  std::string storage_dir_;
  std::string content_dir_;
  const int max_num_installations_;
  const int lowest_priority_;
  const int highest_priority_;
};

InstallationManager::InstallationManager(int max_num_installations,
                                         const std::string& app_key)
    : initialized_(false),
      current_installation_(-1),
      app_key_(app_key),
      max_num_installations_(max_num_installations),
      lowest_priority_(max_num_installations_ - 1),
      highest_priority_(0) {
  SB_CHECK(max_num_installations_ >= 2);
  SB_CHECK(!app_key.empty());
  SB_LOG(INFO) << "InstallationManager: app_key=" << app_key_;
}

int InstallationManager::Initialize() {
  if (initialized_) {
    return IM_ERROR;
  }
  if (!InitInstallationStorePath()) {
    SB_LOG(ERROR) << "Initialize: failed to init paths";
    return IM_ERROR;
  }

  // If there is no existing store, create one.
  if (!LoadInstallationStore()) {
    CreateInstallationStore();
    current_installation_ = FindCurrentInstallationIndex();
    if (!IsValidIndex(current_installation_)) {
      SB_LOG(ERROR) << "Initialize: Unable to find current installation"
                    << current_installation_;
      return IM_ERROR;
    }
    if (!CreateInstallationDirs()) {
      SB_LOG(ERROR) << "Initialize: Unable to create installations dirs";
      return IM_ERROR;
    }
    initialized_ = SaveInstallationStore();
  } else {
    current_installation_ = FindCurrentInstallationIndex();
    if (!IsValidIndex(current_installation_)) {
      SB_LOG(ERROR) << "Initialize: Unable to find current installation"
                    << current_installation_;
      return IM_ERROR;
    }
    initialized_ = true;
  }
  if (!initialized_) {
    SB_LOG(ERROR) << "Initialize: failed";
  }
  SB_DLOG(INFO) << DumpInstallationSlots();
  return initialized_ ? IM_SUCCESS : IM_ERROR;
}

int InstallationManager::GetAppKey(char* app_key, int app_key_length) {
  SB_LOG(INFO) << "InstallationManager::GetAppKey";
  if (!initialized_) {
    SB_LOG(ERROR) << "GetAppKey: not initialized";
    return IM_ERROR;
  }
  starboard::strlcpy(app_key, app_key_.c_str(), app_key_length);
  return IM_SUCCESS;
}

int InstallationManager::GetMaxNumberInstallations() {
  if (!initialized_) {
    SB_LOG(ERROR) << "GetMaxNumberInstallations: not initialized";
    return IM_ERROR;
  }
  return max_num_installations_;
}

int InstallationManager::Reset() {
  CreateInstallationStore();
  current_installation_ = FindCurrentInstallationIndex();
  if (!IsValidIndex(current_installation_)) {
    SB_LOG(ERROR) << "Reset: Unable to find current installation"
                  << current_installation_;
    return IM_ERROR;
  }
  if (!CleanInstallationDirs()) {
    SB_LOG(ERROR) << "Reset: Unable to clean installations dirs";
    return IM_ERROR;
  }
  if (!SaveInstallationStore()) {
    return IM_ERROR;
  }
  return IM_SUCCESS;
}

void InstallationManager::CreateInstallationStore() {
  installation_store_ = cobalt::loader::InstallationStore();
  int priority = highest_priority_;
  for (int i = 0; i < max_num_installations_; i++) {
    installation_store_.add_installations();
    installation_store_.mutable_installations(i)->set_is_successful(false);
    installation_store_.mutable_installations(i)->set_num_tries_left(
        IM_MAX_NUM_TRIES);
    installation_store_.mutable_installations(i)->set_priority(priority++);
  }
  installation_store_.set_roll_forward_to_installation(-1);
  // Mark the system image as successful.
  installation_store_.mutable_installations(0)->set_is_successful(true);
}

std::string InstallationManager::DumpInstallationSlots() {
  std::ostringstream out;
  out << "size=";
  const int kBufSize = 50;
  char buf_num[kBufSize];
  SbStringFormatF(buf_num, kBufSize, "%d",
                  installation_store_.installations_size());
  out << buf_num;

  out << " roll_forward_to_installation=";
  SbStringFormatF(buf_num, kBufSize, "%d",
                  installation_store_.roll_forward_to_installation());
  out << buf_num;
  out << ";";
  for (int i = 0; i < installation_store_.installations_size(); i++) {
    out << " installation_";
    SbStringFormatF(buf_num, kBufSize, "%d", i);
    out << buf_num;
    out << " is_successful=";
    if (installation_store_.installations(i).is_successful()) {
      out << "true";
    } else {
      out << "false";
    }

    out << " num_tries_left=";
    SbStringFormatF(buf_num, kBufSize, "%d",
                    installation_store_.installations(i).num_tries_left());
    out << buf_num;

    out << " priority=";
    SbStringFormatF(buf_num, kBufSize, "%d",
                    installation_store_.installations(i).priority());
    out << buf_num;
    out << ";";
  }
  return out.str();
}

int InstallationManager::GetInstallationStatus(int installation_index) {
  if (!initialized_) {
    SB_LOG(ERROR) << "GetInstallationStatus: not initialized";
    return IM_INSTALLATION_STATUS_ERROR;
  }
  if (!IsValidIndex(installation_index)) {
    SB_LOG(ERROR) << "GetInstallationStatus: invalid index: "
                  << installation_index;
    return IM_INSTALLATION_STATUS_ERROR;
  }
  return installation_store_.installations(installation_index).is_successful();
}

int InstallationManager::GetInstallationNumTriesLeft(int installation_index) {
  if (!initialized_) {
    SB_LOG(ERROR) << "GetInstallationNumTriesLeft: not initialized";
    return IM_ERROR;
  }

  if (!IsValidIndex(installation_index)) {
    SB_LOG(ERROR) << "GetInstallationNumTriesLeft: invalid index: "
                  << installation_index;
    return IM_ERROR;
  }
  return installation_store_.installations(installation_index).num_tries_left();
}

int InstallationManager::DecrementInstallationNumTries(int installation_index) {
  if (!initialized_) {
    SB_LOG(ERROR) << "DecrementInstallationNumTries: not initialized";
    return IM_ERROR;
  }
  if (!IsValidIndex(installation_index)) {
    SB_LOG(ERROR) << "DecrementInstallationNumTries: invalid index: "
                  << installation_index;
    return IM_ERROR;
  }

  int num_tries_left =
      installation_store_.installations(installation_index).num_tries_left();
  if (num_tries_left <= 0) {
    return IM_ERROR;
  }
  installation_store_.mutable_installations(installation_index)
      ->set_num_tries_left(--num_tries_left);

  if (!SaveInstallationStore()) {
    SB_LOG(ERROR) << "DecrementInstallationNumTries: failed to save store";
    return IM_ERROR;
  }
  return IM_SUCCESS;
}

//
// Revert to a previous successful installation and make it
// the highest priority. The current installation becomes the
// lowest priority.
//
//     high [-]    [x]
//          [ ]    [ ]
//          [x] => [ ]
//     low  [ ]    [-]
//
int InstallationManager::RevertToSuccessfulInstallation() {
  if (!initialized_) {
    SB_LOG(ERROR) << "RevertToSuccessfulInstallation: not initialized";
    return IM_ERROR;
  }
  int fallback_priority = lowest_priority_;
  int fallback_installation = -1;

  SB_DLOG(INFO) << "RevertToSuccessfulInstallation: Start "
                << DumpInstallationSlots();
  // Find the highest priority successful installation for fallback.
  for (int i = 0; i < installation_store_.installations().size(); i++) {
    int priority = installation_store_.installations(i).priority();
    if (installation_store_.installations(i).is_successful() &&
        priority > highest_priority_ && priority <= fallback_priority) {
      fallback_priority = priority;
      fallback_installation = i;
      SB_DLOG(INFO) << "fallback_installation= " << fallback_installation;
    }
  }

  if (fallback_installation == -1) {
    SB_LOG(ERROR) << "RevertToSuccessfulInstallation: Unable to find fallback "
                     "installation";
    return IM_ERROR;
  }

  // Shift up all the priorities below the fallback installation up.
  ShiftPrioritiesInRange(fallback_priority, lowest_priority_,
                         -1 /* shift up -1 */);

  // Disable current installation.
  installation_store_.mutable_installations(current_installation_)
      ->set_priority(lowest_priority_);

  // Move the current installation at the bottom.
  installation_store_.mutable_installations(current_installation_)
      ->set_is_successful(false);

  // Change current installation.
  installation_store_.mutable_installations(fallback_installation)
      ->set_priority(highest_priority_);
  current_installation_ = fallback_installation;

  SB_DLOG(INFO) << "RevertToSuccessfulInstallation: End "
                << DumpInstallationSlots();

  if (SaveInstallationStore()) {
    return fallback_installation;
  }
  return IM_ERROR;
}

int InstallationManager::RollForward(int installation_index) {
  if (!initialized_) {
    SB_LOG(ERROR) << "RollForward: not initialized";
    return IM_ERROR;
  }

  if (!IsValidIndex(installation_index)) {
    SB_LOG(ERROR) << "RollForward: invalid installation=" << installation_index;
    return IM_ERROR;
  }
  SB_DLOG(INFO) << "RollForward: installation=" << installation_index;

  return RollForwardInternal(installation_index);
}

int InstallationManager::RollForwardIfNeeded() {
  if (!initialized_) {
    SB_LOG(ERROR) << "RollForwardIfNeeded: not initialized";
    return IM_ERROR;
  }
  //  Check if we need to roll_forward to new slot.
  int new_installation = installation_store_.roll_forward_to_installation();
  if (new_installation == -1) {
    // No need to roll forward.
    return IM_SUCCESS;
  }
  if (!IsValidIndex(new_installation)) {
    SB_LOG(ERROR) << "RollForwardIfNeeded: invalid new_installation="
                  << new_installation;
    return IM_ERROR;
  }
  SB_DLOG(INFO) << "RollForwardIfNeeded: new_installation=" << new_installation;

  return RollForwardInternal(new_installation);
}

//
// Roll forward to a new installation and make it
// the new highest priority installation.
//
//     high [ ]    [x]
//          [ ]    [ ]
//          [x] => [ ]
//     low  [ ]    [ ]
//
int InstallationManager::RollForwardInternal(int installation_index) {
  // Save old priority.
  int new_installation_old_priority =
      installation_store_.installations(installation_index).priority();

  SB_DLOG(INFO) << "RollForwardInternal: new_installation_old_priority="
                << new_installation_old_priority;

  // Lower priorities of all jumped over installations.
  ShiftPrioritiesInRange(highest_priority_, new_installation_old_priority,
                         1 /* shift down +1 */);

  // The new installation will be set to the highest priority.
  installation_store_.mutable_installations(installation_index)
      ->set_priority(highest_priority_);

  // Reset the roll forward index.
  installation_store_.set_roll_forward_to_installation(-1);
  current_installation_ = installation_index;

  SB_DLOG(INFO) << "RollForwardInternal: " << DumpInstallationSlots();
  return SaveInstallationStore() ? IM_SUCCESS : IM_ERROR;
}

// Shift the priority in the inclusive range either up or down based
// on the |direction_up" flag.
void InstallationManager::ShiftPrioritiesInRange(int high_priority,
                                                 int low_priority,
                                                 int shift_amount) {
  for (int i = 0; i < installation_store_.installations().size(); i++) {
    int priority = installation_store_.installations(i).priority();
    if (priority >= high_priority && priority <= low_priority) {
      installation_store_.mutable_installations(i)->set_priority(priority +
                                                                 shift_amount);
      SB_DLOG(INFO) << "ShiftPrioritiesInRange i=" << i
                    << " priority_new=" << priority;
    }
  }
}

int InstallationManager::GetInstallationPath(int installation_index,
                                             char* path,
                                             int path_length) {
  if (!initialized_) {
    SB_LOG(ERROR) << "GetInstallationPath: not initialized";
    return IM_ERROR;
  }
  return GetInstallationPathInternal(installation_index, path, path_length)
             ? IM_SUCCESS
             : IM_ERROR;
}

int InstallationManager::GetCurrentInstallationIndex() {
  if (!initialized_) {
    SB_LOG(ERROR) << "GetCurrentInstallationIndex: not initialized";
    return IM_ERROR;
  }
  return current_installation_;
}

void InstallationManager::LogLastSystemError(const char* msg) {
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];
  SbSystemError error_code = SbSystemGetLastError();
  if (SbSystemGetErrorString(error_code, msgbuf, kErrorMessageBufferSize) > 0) {
    SB_LOG(ERROR) << msg << ": " << msgbuf;
  }
}

int InstallationManager::FindCurrentInstallationIndex() {
  int highest_priority_index = -1;
  for (int i = 0; i < installation_store_.installations_size(); i++) {
    if (highest_priority_ == installation_store_.installations(i).priority()) {
      highest_priority_index = i;
      break;
    }
  }

  return highest_priority_index;
}

int InstallationManager::MarkInstallationSuccessful(int installation_index) {
  if (!initialized_) {
    SB_LOG(ERROR) << "MarkInstallationSuccessful: not initialized";
    return IM_ERROR;
  }
  if (!IsValidIndex(installation_index)) {
    SB_LOG(ERROR) << "MarkInstallationSuccessful: invalid index"
                  << installation_index;
    return IM_ERROR;
  }
  if (!installation_store_.installations(installation_index).is_successful()) {
    installation_store_.mutable_installations(installation_index)
        ->set_is_successful(true);
    return SaveInstallationStore() ? IM_SUCCESS : IM_ERROR;
  }
  return IM_SUCCESS;
}

int InstallationManager::SelectNewInstallationIndex() {
  if (!initialized_) {
    SB_LOG(ERROR) << "SelectNewInstallationIndex: not initialized";
    return IM_ERROR;
  }

  int priority = highest_priority_;
  int new_installation_index = -1;

  // SLOT_0 is placed in |kSbSystemPathContentDirectory|, under the subdirectory
  // 'app/cobalt', and is always the system image.
  int start = 1;

  // Find the lowest priority installation that we can use.
  for (int i = start; i < installation_store_.installations().size(); i++) {
    if (priority < installation_store_.installations(i).priority()) {
      new_installation_index = i;
      priority = installation_store_.installations(i).priority();
      SB_DLOG(INFO) << "SelectNewInstallationIndex: lowest_priority_index= "
                    << new_installation_index;
    }
  }

  if (new_installation_index != -1) {
    installation_store_.mutable_installations(new_installation_index)
        ->set_is_successful(false);
    installation_store_.mutable_installations(new_installation_index)
        ->set_num_tries_left(IM_MAX_NUM_TRIES);

    if (!SaveInstallationStore()) {
      SB_LOG(ERROR) << "SelectNewInstallationIndex: failed to store";
      return IM_ERROR;
    }

    SB_DLOG(INFO) << "SelectNewInstallationIndex: " << DumpInstallationSlots();
    return new_installation_index;
  }
  return IM_ERROR;
}

int InstallationManager::ResetInstallation(int installation_index) {
  if (!initialized_) {
    SB_LOG(ERROR) << "ResetInstallation: not initialized";
    return IM_ERROR;
  }

  // invalid index or the system image.
  if (installation_index == -1 || installation_index == 0) {
    return IM_ERROR;
  }

  installation_store_.mutable_installations(installation_index)
      ->set_is_successful(false);
  installation_store_.mutable_installations(installation_index)
      ->set_num_tries_left(IM_MAX_NUM_TRIES);

  if (!SaveInstallationStore()) {
    SB_LOG(ERROR) << "ResetInstallation: failed to store";
    return IM_ERROR;
  }

  SB_DLOG(INFO) << "ResetInstallation: " << DumpInstallationSlots();
  return IM_SUCCESS;
}

int InstallationManager::RequestRollForwardToInstallation(
    int installation_index) {
  if (!initialized_) {
    SB_LOG(ERROR) << "RequestRollForwardToInstallation: not initialized";
    return IM_ERROR;
  }
  if (!IsValidIndex(installation_index)) {
    SB_LOG(ERROR) << "RequestRollForwardToInstallation: invalid index"
                  << installation_index;
    return IM_ERROR;
  }
  installation_store_.set_roll_forward_to_installation(installation_index);
  return SaveInstallationStore() ? IM_SUCCESS : IM_ERROR;
}

bool InstallationManager::SaveInstallationStore() {
  ValidatePriorities();

  if (IM_MAX_INSTALLATION_STORE_SIZE < installation_store_.ByteSize()) {
    SB_LOG(ERROR) << "SaveInstallationStore: Data too large"
                  << installation_store_.ByteSize();
    return false;
  }

  const size_t buf_size = installation_store_.ByteSize();
  std::vector<char> buf(buf_size, 0);

  int result = installation_store_.roll_forward_to_installation();
#if !SB_IS(EVERGREEN_COMPATIBLE_LITE)
  loader_app::SetPendingRestart(result != -1);
#endif

  installation_store_.SerializeToArray(buf.data(),
                                       installation_store_.ByteSize());

#if SB_API_VERSION >= 12
  if (!SbFileAtomicReplace(store_path_.c_str(), buf.data(),
                           installation_store_.ByteSize())) {
    SB_LOG(ERROR)
        << "SaveInstallationStore: Failed to store installation store: "
        << store_path_;
    return false;
  }
  SB_DLOG(INFO) << "SaveInstallationStore successful";
  return true;

#else
  SB_NOTREACHED()
      << "SbFileAtomicReplace is not available before starboard version 12";
  return false;
#endif
}

bool InstallationManager::InitInstallationStorePath() {
  std::vector<char> storage_dir(kSbFileMaxPath);
#if SB_API_VERSION >= 12
  if (!SbSystemGetPath(kSbSystemPathStorageDirectory, storage_dir.data(),
                       kSbFileMaxPath)) {
    SB_LOG(ERROR) << "InitInstallationStorePath: Failed to get "
                     "kSbSystemPathStorageDirectory";
    return false;
  }
#else
  SB_NOTREACHED() << "InitInstallationStorePath: kSbSystemPathStorageDirectory "
                     "is not available before "
                     "starboard version "
                  << 12;
  return false;

#endif
  storage_dir_ = storage_dir.data();
  store_path_ = storage_dir.data();
  store_path_ += kSbFileSepString;
  store_path_ += IM_STORE_FILE_NAME_PREFIX;
  store_path_ += app_key_;
  store_path_ += IM_STORE_FILE_NAME_SUFFIX;

  std::vector<char> content_dir(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathContentDirectory, content_dir.data(),
                       kSbFileMaxPath)) {
    return false;
  }
  content_dir_ = content_dir.data();
  return true;
}

bool InstallationManager::IsValidIndex(int index) {
  return index >= 0 && index < max_num_installations_;
}

void InstallationManager::ValidatePriorities() {
  std::set<int> priorities;
  for (int i = 0; i < max_num_installations_; i++) {
    priorities.insert(installation_store_.installations(i).priority());
  }
  for (int i = 0; i < max_num_installations_; i++) {
    SB_DCHECK(priorities.find(i) != priorities.end());
  }
  SB_DCHECK(priorities.size() == max_num_installations_);
}

bool InstallationManager::LoadInstallationStore() {
  SbFile file;

  SB_LOG(INFO) << "StorePath=" << store_path_;
  file = SbFileOpen(store_path_.c_str(), kSbFileOpenOnly | kSbFileRead, NULL,
                    NULL);
  if (!file) {
    SB_LOG(WARNING) << "Failed to open file: " << store_path_;
    return false;
  }

  char buf[IM_MAX_INSTALLATION_STORE_SIZE];
  int count = SbFileReadAll(file, buf, IM_MAX_INSTALLATION_STORE_SIZE);
  SB_DLOG(INFO) << "SbFileReadAll: count=" << count;
  if (count == -1) {
    LogLastSystemError("SbFileReadAll failed");
    return false;
  }
  if (!installation_store_.ParseFromArray(buf, count)) {
    SB_LOG(ERROR) << "LoadInstallationStore: Unable to parse storage";
    return false;
  }
  SbFileClose(file);
  return true;
}

bool InstallationManager::GetInstallationPathInternal(int installation_index,
                                                      char* path,
                                                      int path_length) {
  if (!IsValidIndex(installation_index)) {
    SB_LOG(ERROR) << "GetInstallationPath: invalid index" << installation_index;
    return false;
  }
  if (!path) {
    SB_LOG(ERROR) << "GetInstallationPath: path is null";
    return false;
  }
  // SLOT_0 is placed in |kSbSystemPathContentDirectory|, under the subdirectory
  // 'app/cobalt'.
  if (installation_index == 0) {
    SbStringFormatF(path, path_length, "%s%s%s%s%s", content_dir_.c_str(),
                    kSbFileSepString, "app", kSbFileSepString, "cobalt");
  } else {
    SbStringFormatF(path, path_length, "%s%s%s%d", storage_dir_.c_str(),
                    kSbFileSepString, "installation_", installation_index);
  }

  return true;
}

bool InstallationManager::CreateInstallationDirs() {
  std::vector<char> path(kSbFileMaxPath);
  for (int i = 0; i < max_num_installations_; i++) {
    // SLOT_0 is placed in |kSbSystemPathContentDirectory|, under the
    // subdirectory 'app/cobalt'.
    if (i == 0) {
      continue;
    }
    if (!GetInstallationPathInternal(i, path.data(), kSbFileMaxPath)) {
      return false;
    }

    if (!SbDirectoryCreate(path.data())) {
      return false;
    }
  }
  return true;
}

bool InstallationManager::CleanInstallationDirs() {
  std::vector<char> path(kSbFileMaxPath);
  // The index 0 slot is under the content directory.
  for (int i = 1; i < max_num_installations_; i++) {
    if (!GetInstallationPathInternal(i, path.data(), kSbFileMaxPath)) {
      return false;
    }
    if (!SbFileDeleteRecursive(path.data(), true)) {
      return false;
    }
  }
  return true;
}

}  // namespace installation_manager
}  // namespace loader_app
}  // namespace starboard

#ifdef __cplusplus
extern "C" {
#endif

starboard::scoped_ptr<
    starboard::loader_app::installation_manager::InstallationManager>
    g_installation_manager_;

// Global Installation Manager Mutex.
SB_ONCE_INITIALIZE_FUNCTION(starboard::Mutex, GetImMutex);

int ImInitialize(int max_num_installations, const char* app_key) {
  starboard::ScopedLock lock(*GetImMutex());
  if (g_installation_manager_.get() == NULL) {
    g_installation_manager_.reset(
        new starboard::loader_app::installation_manager::InstallationManager(
            max_num_installations, app_key));
  }
  return g_installation_manager_->Initialize();
}

int ImGetAppKey(char* app_key, int app_key_length) {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->GetAppKey(app_key, app_key_length);
}

int ImGetMaxNumberInstallations() {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->GetMaxNumberInstallations();
}

void ImUninitialize() {
  starboard::ScopedLock lock(*GetImMutex());
  g_installation_manager_.reset(NULL);
}

int ImReset() {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->Reset();
}

int ImGetInstallationStatus(int installation_index) {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->GetInstallationStatus(installation_index);
}

int ImGetInstallationNumTriesLeft(int installation_index) {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->GetInstallationNumTriesLeft(
      installation_index);
}

int ImDecrementInstallationNumTries(int installation_index) {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->DecrementInstallationNumTries(
      installation_index);
}

int ImGetCurrentInstallationIndex() {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->GetCurrentInstallationIndex();
}

int ImResetInstallation(int installation_index) {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->ResetInstallation(installation_index);
}

int ImSelectNewInstallationIndex() {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->SelectNewInstallationIndex();
}

int ImGetInstallationPath(int installation_index, char* path, int path_length) {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->GetInstallationPath(installation_index, path,
                                                      path_length);
}

int ImMarkInstallationSuccessful(int installation_index) {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->MarkInstallationSuccessful(
      installation_index);
}

int ImRollForwardIfNeeded() {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->RollForwardIfNeeded();
}

int ImRollForward(int installation_index) {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->RollForward(installation_index);
}

int ImRevertToSuccessfulInstallation() {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->RevertToSuccessfulInstallation();
}

int ImRequestRollForwardToInstallation(int installation_index) {
  starboard::ScopedLock lock(*GetImMutex());
  return g_installation_manager_->RequestRollForwardToInstallation(
      installation_index);
}

#ifdef __cplusplus
}  // extern "C"
#endif
