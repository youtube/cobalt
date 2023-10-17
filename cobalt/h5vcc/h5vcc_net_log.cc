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
#include "cobalt/network/cobalt_net_log.h"

namespace cobalt {
namespace h5vcc {

namespace {
const char* kOutputNetLogFilename = "h5vcc_netlog.json";
}  // namespace

H5vccNetLog::H5vccNetLog(cobalt::network::NetworkModule* network_module)
    : network_module_{network_module} {
  base::FilePath result;
  base::PathService::Get(cobalt::paths::DIR_COBALT_DEBUG_OUT, &result);
  absolute_log_path_ = result.Append(kOutputNetLogFilename);
}

void H5vccNetLog::Start() {
  LOG(INFO) << "YO THOR! STRATING H5VCC NET LGO ";
  net::NetLogCaptureMode capture_mode;  // default
  network_module_->StartNetLog(absolute_log_path_, capture_mode);
}

void H5vccNetLog::Stop() {
  LOG(INFO) << "YO THOR! STOP H5VCC NET LGO ";
  network_module_->StopNetLog();
}

std::string H5vccNetLog::Read() {
  LOG(INFO) << "YO THOR! READ H5VCC NET LGO ";
  Stop();

  std::string netlog_output{};
  if (!absolute_log_path_.empty()) {
    ReadFileToString(absolute_log_path_, &netlog_output);
  }
  return netlog_output;
}


}  // namespace h5vcc
}  // namespace cobalt
