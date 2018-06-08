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

#include "starboard/shared/win32/minidump.h"

#include <windows.h>  // Has to go first.
#include <crtdbg.h>
#include <dbghelp.h>
#include <string>

#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/shared/win32/file_internal.h"

using starboard::Mutex;
using starboard::ScopedLock;
using starboard::shared::win32::NormalizeWin32Path;

namespace starboard {
namespace shared {
namespace win32 {

namespace {

class DumpHandler {
 public:
  static DumpHandler* Instance();

  void Init(std::string file_path) {
    ScopedLock lock(mutex_);
    if (initialized_) {
      return;
    }
    file_path_ = file_path;
    // After this call, unhandled exceptions will use the exception handler
    // when an error occurs but only if the debugger is not attached.
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);

    std::stringstream ss;
    ss << "\n****MiniDumpHandler activated***\nIf a crash happens then an "
       << "attempt to write a MiniDump file to: " << file_path << "\n\n";
    SbLogRaw(ss.str().c_str());

    initialized_ = true;
  }

 private:
  static LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* pep) {
    DumpHandler::Instance()->DumpStack(pep);
    return EXCEPTION_EXECUTE_HANDLER;
  }
  DumpHandler() {}

  void DumpStack(EXCEPTION_POINTERS* pep) {
    ScopedLock lock(mutex_);
    if (file_path_.empty()) {
      SbLogRaw("Could not write minidump because the dump path is missing.");
      return;
    }
    bool out_created = false;
    SbFileError out_error = kSbFileOk;

    HANDLE file_handle = OpenFileOrDirectory(
        file_path_.c_str(), kSbFileCreateAlways | kSbFileWrite,
        &out_created, &out_error);

    const bool file_ok = out_created && (out_error == kSbFileOk) &&
                         (file_handle != NULL) &&
                         (file_handle != INVALID_HANDLE_VALUE);

    if (!file_ok) {
      std::stringstream ss;
      ss << "CreateFile failed. Error: " << GetLastError() << "\n";
      SbLogRaw(ss.str().c_str());
      return;
    }

    // Create the minidump.
    MINIDUMP_EXCEPTION_INFORMATION mdei;
    mdei.ThreadId           = GetCurrentThreadId();
    mdei.ExceptionPointers  = pep;
    mdei.ClientPointers     = TRUE;
    MINIDUMP_TYPE mdt =
        static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory |
                                   MiniDumpWithFullMemoryInfo |
                                   MiniDumpWithHandleData |
                                   MiniDumpWithThreadInfo |
                                   MiniDumpWithUnloadedModules);

    BOOL rv = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                file_handle, mdt,
                                (pep != 0) ? &mdei : 0,
                                0, 0);
    std::stringstream ss;
    if (!rv) {
      ss << "Minidump write failed. Error: " << GetLastError() << "\n";
    } else {
      ss << "Minidump " << file_path_ << "created.\n";
    }
    // Lower level loggin than SbLogRaw().
    SbLogRaw(ss.str().c_str());
    CloseHandle(file_handle);
  }

  std::string file_path_;
  starboard::Mutex mutex_;
  bool initialized_ = false;
};

SB_ONCE_INITIALIZE_FUNCTION(DumpHandler, DumpHandler::Instance);

}  // namespace

void InitMiniDumpHandler(const char* file_path) {
#ifndef COBALT_BUILD_TYPE_GOLD
  DumpHandler::Instance()->Init(file_path);
#endif
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
