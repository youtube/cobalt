// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_net_log.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"

namespace cobalt {
namespace h5vcc {

namespace {
const char* kOutputNetLogFilename = "h5vcc_netlog.json";

base::FilePath filepath_to_absolute(
    const base::FilePath& output_path_relative_to_logs) {
  base::FilePath result;
  base::PathService::Get(cobalt::paths::DIR_COBALT_DEBUG_OUT, &result);
  LOG(INFO) << "YO THOR! DEBUG OUT is:" << result.value();
  return result.Append(output_path_relative_to_logs);
}

}  // namespace

H5vccNetLog::H5vccNetLog() {}

void H5vccNetLog::Start(const std::string& output_filename) {
  LOG(INFO) << "YO THOR - NET LGO START!";
  if (net_log_) {
    DLOG(WARNING) << "H5vccNetLog is already started.";
  } else {
    base::FilePath net_log_path(output_filename.empty() ? kOutputNetLogFilename
                                                        : output_filename);
    LOG(INFO) << "YO THOR! FILENAME IS:" << output_filename;
    net::NetLogCaptureMode capture_mode;  // default
    last_absolute_path_ = filepath_to_absolute(base::FilePath(output_filename));
    net_log_.reset(
        new network::CobaltNetLog(last_absolute_path_, capture_mode));
  }
}

void H5vccNetLog::Stop() {
  LOG(INFO) << "YO THOR - NET LGO STOP!";
  if (net_log_) {
    net_log_.reset();
  } else {
    DLOG(WARNING) << "H5vccNetLog is already stopped.";
  }
}

std::string H5vccNetLog::Read(const std::string& read_filename) {
  LOG(INFO) << "YO THOR - NET LGO READ :" << read_filename << "!";
  if (net_log_) {
    Stop();
  }
  std::string netlog_output;
  if (!read_filename.empty()) {
    LOG(INFO) << "YO THOR - GOT A FILENAME: " << read_filename;
    auto file_path = filepath_to_absolute(base::FilePath(read_filename));
    auto succeed = ReadFileToString(file_path, &netlog_output);
    LOG(INFO) << "YO THOR - did ya read OK?? " << (succeed ? "TRUE" : "fALSE");
  } else if (!last_absolute_path_.empty()) {
    LOG(INFO) << "YO THOR - ELSE - GOT A ABS PATH";
    auto succeed = ReadFileToString(last_absolute_path_, &netlog_output);
    LOG(INFO) << "YO THOR - did ya read OK? " << (succeed ? "TRUE" : "fALSE");
  }
  if (!last_absolute_path_.empty()) {
    LOG(INFO) << "YO THOR - VERIFY - GOT A ABS PATH";
    auto succeed = ReadFileToString(last_absolute_path_, &netlog_output);
    LOG(INFO) << "YO THOR - did ya read OK? " << (succeed ? "TRUE" : "fALSE");
  }
  return netlog_output;
}


}  // namespace h5vcc
}  // namespace cobalt
