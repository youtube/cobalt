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

#include <string>

#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/string.h"

using Windows::Foundation::AsyncOperationCompletedHandler;
using Windows::Foundation::AsyncStatus;
using Windows::Foundation::IAsyncOperation;
using Windows::Storage::FileAccessMode;
using Windows::Storage::StorageFile;
using Windows::Storage::StorageFolder;
using Windows::Storage::Streams::DataWriter;
using Windows::Storage::Streams::IRandomAccessStream;
using Windows::Storage::Streams::IOutputStream;

namespace {

SbMutex log_mutex_ = SB_MUTEX_INITIALIZER;

// The Windows Storage API must be used in order to access files in priviledged
// areas (e.g. KnownFolders::RemovableDevices). The win32 file API used by
// SbFile returns access denied errors in these situations.
DataWriter^ log_writer_ = nullptr;

// SbMutex is not reentrant, so factor out close log file functionality for use
// by other functions.
void CloseLogFileWithoutLock() {
  log_writer_ = nullptr;
}

}  // namespace

namespace starboard {
namespace shared {
namespace uwp {

void CloseLogFile() {
  SbMutexAcquire(&log_mutex_);
  CloseLogFileWithoutLock();
  SbMutexRelease(&log_mutex_);
}

void OpenLogFile(StorageFolder^ folder, const char* filename) {
  std::wstring wfilename = win32::CStringToWString(filename);

  SbMutexAcquire(&log_mutex_);
  CloseLogFileWithoutLock();

  // Manually set the completion callback function instead of using
  // concurrency::create_task() since those tasks may not execute before the
  // UI thread wants the log_mutex_ to output another log.
  auto task = folder->CreateFileAsync(
      ref new Platform::String(wfilename.c_str()),
      Windows::Storage::CreationCollisionOption::ReplaceExisting);
  task->Completed = ref new AsyncOperationCompletedHandler<StorageFile^>(
      [folder](IAsyncOperation<StorageFile^>^ op, AsyncStatus) {
      try {
        auto task = op->GetResults()->OpenAsync(FileAccessMode::ReadWrite);
        task->Completed = ref new
            AsyncOperationCompletedHandler<IRandomAccessStream^>(
            [](IAsyncOperation<IRandomAccessStream^>^ op, AsyncStatus) {
              log_writer_ = ref new DataWriter(
                  op->GetResults()->GetOutputStreamAt(0));
              SbMutexRelease(&log_mutex_);
            });
      } catch(Platform::Exception^) {
        SbMutexRelease(&log_mutex_);
        SB_LOG(ERROR) << "Unable to open log file in folder "
                      << win32::platformStringToString(folder->Name);
      }
    });
}

void WriteToLogFile(const char* text, int text_length) {
  if (text_length <= 0) {
    return;
  }

  SbMutexAcquire(&log_mutex_);
  if (log_writer_) {
    log_writer_->WriteBytes(ref new Platform::Array<unsigned char>(
        (unsigned char*) text, text_length));

    // Manually set the completion callback function instead of using
    // concurrency::create_task() since those tasks may not execute before the
    // UI thread wants the log_mutex_ to output another log.
    auto task = log_writer_->StoreAsync();
    task->Completed = ref new AsyncOperationCompletedHandler<unsigned int>(
        [](IAsyncOperation<unsigned int>^, AsyncStatus) {
          auto task = log_writer_->FlushAsync();
          task->Completed = ref new AsyncOperationCompletedHandler<bool>(
              [](IAsyncOperation<bool>^, AsyncStatus) {
                SbMutexRelease(&log_mutex_);
              });
        });
  } else {
    SbMutexRelease(&log_mutex_);
  }
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
