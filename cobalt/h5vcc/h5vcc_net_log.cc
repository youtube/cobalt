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

H5vccNetLog::H5vccNetLog(cobalt::network::NetworkModule* network_module)
    : network_module_{network_module} {}

void H5vccNetLog::Start() { network_module_->StartNetLog(); }

void H5vccNetLog::Stop() { network_module_->StopNetLog(); }

std::string H5vccNetLog::Read() {
  base::FilePath netlog_path = network_module_->StopNetLog();
  std::string netlog_output{};
  if (!netlog_path.empty()) {
    ReadFileToString(netlog_path, &netlog_output);
  }
  return netlog_output;
}


}  // namespace h5vcc
}  // namespace cobalt
