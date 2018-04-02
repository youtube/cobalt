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

#include "starboard/shared/uwp/log_writer_uwp.h"

#include <string>

#include "starboard/common/semaphore.h"
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

namespace starboard {
namespace shared {
namespace uwp {
namespace {

class SharedMutex {
 public:
  SharedMutex() : sema_(1) {}
  void Acquire() { sema_.Take(); }
  void Release() { sema_.Put(); }
  bool AcquireTry() { return sema_.TakeTry(); }
 private:
  Semaphore sema_;
};

class LogWriterUWP : public ILogWriter {
 public:
  LogWriterUWP(StorageFolder^ folder, const char* filename) {
    OpenLogFile(folder, filename);
  }

  ~LogWriterUWP() {
    CloseLogFile();
  }

  void Write(const char* content, int size) override {
    WriteToLogFile(content, size);
  }

 private:
  // SbMutex is not reentrant, so factor out close log file functionality for
  // use by other functions.
  void CloseLogFile_Locked() {
    log_writer_ = nullptr;
  }

  void CloseLogFile() {
    log_mutex_.Acquire();
    CloseLogFile_Locked();
    log_mutex_.Release();
  }

  void OpenLogFile(StorageFolder^ folder, const char* filename) {
    std::wstring wfilename = win32::CStringToWString(filename);

    log_mutex_.Acquire();
    CloseLogFile_Locked();

    // Manually set the completion callback function instead of using
    // concurrency::create_task() since those tasks may not execute before the
    // UI thread wants the log_mutex_ to output another log.
    auto task = folder->CreateFileAsync(
        ref new Platform::String(wfilename.c_str()),
        Windows::Storage::CreationCollisionOption::ReplaceExisting);
    task->Completed = ref new AsyncOperationCompletedHandler<StorageFile^>(
        [folder, this](IAsyncOperation<StorageFile^>^ op, AsyncStatus) {
        if (op->Status != AsyncStatus::Completed) {
          this->log_mutex_.Release();
          SB_LOG(ERROR) << "Unable to open log file in folder "
                        << win32::platformStringToString(folder->Name);
          return;
        }

        try {
          auto task = op->GetResults()->OpenAsync(FileAccessMode::ReadWrite);
          task->Completed = ref new
              AsyncOperationCompletedHandler<IRandomAccessStream^>(
              [this](IAsyncOperation<IRandomAccessStream^>^ op, AsyncStatus) {
                this->log_writer_ = ref new DataWriter(
                    op->GetResults()->GetOutputStreamAt(0));
                this->log_mutex_.Release();
              });
        } catch(Platform::Exception^) {
          this->log_mutex_.Release();
          SB_LOG(ERROR) << "Unable to open log file in folder "
                        << win32::platformStringToString(folder->Name);
        }
      });
  }

  void WriteToLogFile(const char* text, int text_length) {
    if (text_length <= 0) {
      return;
    }
    log_mutex_.Acquire();
    if (log_writer_) {
      log_writer_->WriteBytes(ref new Platform::Array<unsigned char>(
          (unsigned char*) text, text_length));

      // Manually set the completion callback function instead of using
      // concurrency::create_task() since those tasks may not execute before the
      // UI thread wants the log_mutex_ to output another log.
      auto task = log_writer_->StoreAsync();
      task->Completed = ref new AsyncOperationCompletedHandler<unsigned int>(
          [this](IAsyncOperation<unsigned int>^, AsyncStatus) {
            auto task = this->log_writer_->FlushAsync();
            task->Completed = ref new AsyncOperationCompletedHandler<bool>(
                [this](IAsyncOperation<bool>^, AsyncStatus) {
                  this->log_mutex_.Release();
                });
          });
    } else {
      log_mutex_.Release();
    }
  }
  SharedMutex log_mutex_;
  // The Windows Storage API must be used in order to access files in
  // privileged areas (e.g. KnownFolders::RemovableDevices). The win32
  // file API used by SbFile returns access denied errors in these situations.
  DataWriter^ log_writer_ = nullptr;
};
}  // namespace.

scoped_ptr<ILogWriter> CreateLogWriterUWP(
    Windows::Storage::StorageFolder^ folder, const char* filename) {
  scoped_ptr<ILogWriter> output(new LogWriterUWP(folder, filename));
  return output.Pass();
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
