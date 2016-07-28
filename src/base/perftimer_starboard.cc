// Copyright 2015 Google Inc. All Rights Reserved.
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

// Written for Starboard, but should work for any platform that implements
// base::PlatformFile.

#include "base/perftimer.h"

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/stringprintf.h"

static base::PlatformFile perf_log_file = base::kInvalidPlatformFileValue;

bool InitPerfLog(const FilePath& log_file) {
  if (perf_log_file != base::kInvalidPlatformFileValue) {
    // Avoid double initialization.
    NOTREACHED();
    return false;
  }

  perf_log_file = base::CreatePlatformFile(
      log_file, base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_WRITE,
      NULL, NULL);
  return perf_log_file != base::kInvalidPlatformFileValue;
}

void FinalizePerfLog() {
  if (perf_log_file == base::kInvalidPlatformFileValue) {
    // The caller is trying to cleanup without initializing.
    NOTREACHED();
    return;
  }
  base::ClosePlatformFile(perf_log_file);
}

void LogPerfResult(const char* test_name, double value, const char* units) {
  if (perf_log_file == base::kInvalidPlatformFileValue) {
    NOTREACHED();
    return;
  }

  std::string message =
      base::StringPrintf("%s\t%g\t%s\n", test_name, value, units);
  base::WritePlatformFileAtCurrentPos(perf_log_file, message.c_str(),
                                      message.length());
  DLOG(INFO) << message;
}
