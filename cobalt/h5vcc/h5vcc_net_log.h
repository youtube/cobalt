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

#ifndef COBALT_H5VCC_H5VCC_NET_LOG_H_
#define COBALT_H5VCC_H5VCC_NET_LOG_H_

#include <memory>
#include <string>

#include "cobalt/network/cobalt_net_log.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccNetLog : public script::Wrappable {
 public:
  H5vccNetLog();

  void Start(const std::string& output_filename);
  void Stop();
  std::string Read(const std::string& output_filename);

  DEFINE_WRAPPABLE_TYPE(H5vccNetLog);

 private:
  std::unique_ptr<network::CobaltNetLog> net_log_;

  base::FilePath last_absolute_path_;

  DISALLOW_COPY_AND_ASSIGN(H5vccNetLog);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_NET_LOG_H_
