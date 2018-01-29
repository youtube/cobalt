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

#include "starboard/shared/uwp/watchdog_log.h"

#include <string>

#include "starboard/common/scoped_ptr.h"
#include "starboard/common/semaphore.h"
#include "starboard/file.h"
#include "starboard/log.h"
#include "starboard/shared/win32/simple_thread.h"
#include "starboard/string.h"

using starboard::shared::win32::SimpleThread;

namespace starboard {
namespace shared {
namespace uwp {
namespace {

// The WatchDogThread will print out "alive: <COUNT>\n" periodically to a
// file. On destruction WatchDogThread will write out "done\n".
// This allows a test runner to accurately determine whether this process
// is still alive, finished, or crashed.
class WatchDogThread : public SimpleThread {
 public:
  explicit WatchDogThread(const std::string& file_path)
      : SimpleThread("WatchDogLog"), file_path_(file_path) {
    SimpleThread::Start();
  }

  ~WatchDogThread() {
    Join();
  }

  void Run() override {
    static const SbTime kSleepTime = kSbTimeMillisecond * 250;
    int counter = 0;
    bool created_ok = false;
    SbFileError out_error = kSbFileOk;
    SbFile file_handle = SbFileOpen(file_path_.c_str(),
                                    kSbFileCreateAlways | kSbFileWrite,
                                    &created_ok,
                                    &out_error);
    if (!created_ok) {
      SB_LOG(ERROR) << "Could not create watchdog file " << file_path_;
      return;
    }
    while (!exiting_sema_.TakeWait(kSleepTime)) {
      std::stringstream ss;
      ss << "alive: " << counter++ << "\n";
      std::string str = ss.str();
      SbFileWrite(file_handle, str.c_str(), static_cast<int>(str.size()));
      SbFileFlush(file_handle);
    }
    const char kDone[] = "done\n";
    SbFileWrite(file_handle, kDone, static_cast<int>(SbStringGetLength(kDone)));
    SbFileFlush(file_handle);
    SbThreadSleep(50 * kSbTimeMillisecond);
    bool closed = SbFileClose(file_handle);
    SB_LOG_IF(ERROR, closed) << "Could not close file " << file_path_;
  }

  void Join() override {
    exiting_sema_.Put();
    SimpleThread::Join();
  }

 private:
  std::string file_path_;
  starboard::Semaphore exiting_sema_;
};
starboard::scoped_ptr<WatchDogThread> s_watchdog_singleton_;
}  // namespace.

void StartWatchdogLog(const std::string& path) {
  if (s_watchdog_singleton_.get()) {
    SB_LOG(ERROR) << "WatchDogThread exists, aborting.";
    return;
  }
  s_watchdog_singleton_.reset(new WatchDogThread(path));
}

void CloseWatchdogLog() {
  s_watchdog_singleton_.reset(nullptr);
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
