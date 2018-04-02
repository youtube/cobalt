// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/uwp/log_file_impl.h"

#include <ppltasks.h>
#include <string>

#include "starboard/common/scoped_ptr.h"
#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/shared/uwp/log_writer_uwp.h"
#include "starboard/shared/uwp/log_writer_win32.h"
#include "starboard/string.h"

using Windows::Storage::StorageFolder;

namespace starboard {
namespace shared {
namespace uwp {

namespace {

class LogFileImpl {
 public:
  static LogFileImpl* GetInstance();

  void OpenUWP(StorageFolder^ folder, const char* filename) {
    ScopedLock lock(mutex_);
    impl_.reset();
    impl_ = CreateLogWriterUWP(folder, filename);
  }

  void OpenWin32(const char* path) {
    ScopedLock lock(mutex_);
    impl_.reset();
    impl_ = CreateLogWriterWin32(path);
  }

  void Close() {
    ScopedLock lock(mutex_);
    impl_.reset();
  }

  void Write(const char* text, int text_length) {
    ScopedLock lock(mutex_);
    if (impl_) {
      impl_->Write(text, text_length);
    }
  }

 private:
  LogFileImpl() {}
  starboard::Mutex mutex_;
  starboard::scoped_ptr<ILogWriter> impl_;
};

SB_ONCE_INITIALIZE_FUNCTION(LogFileImpl, LogFileImpl::GetInstance);

}  // namespace

void CloseLogFile() {
  LogFileImpl::GetInstance()->Close();
}

void OpenLogFileUWP(StorageFolder^ folder, const char* filename) {
  LogFileImpl::GetInstance()->OpenUWP(folder, filename);
}

void OpenLogFileWin32(const char* path) {
  LogFileImpl::GetInstance()->OpenWin32(path);
}

void WriteToLogFile(const char* text, int text_length) {
  if (text_length <= 0) {
    return;
  }
  LogFileImpl::GetInstance()->Write(text, text_length);
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
