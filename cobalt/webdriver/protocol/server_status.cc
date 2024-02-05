// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/webdriver/protocol/server_status.h"

#include <memory>
#include <utility>

#include "cobalt/version.h"
#include "starboard/common/system_property.h"

using starboard::kSystemPropertyMaxLength;

namespace cobalt {
namespace webdriver {
namespace protocol {

ServerStatus::ServerStatus() {
  char value[kSystemPropertyMaxLength];
  bool result;

  result = SbSystemGetProperty(kSbSystemPropertyPlatformName, value,
                               kSystemPropertyMaxLength);
  if (result) {
    os_name_ = value;
  }

  result = SbSystemGetProperty(kSbSystemPropertyFirmwareVersion, value,
                               kSystemPropertyMaxLength);
  if (result) {
    os_version_ = value;
  }

  result = SbSystemGetProperty(kSbSystemPropertyChipsetModelNumber, value,
                               kSystemPropertyMaxLength);
  if (result) {
    os_arch_ = value;
  }
  build_time_ = __DATE__ " (" __TIME__ ")";
  build_version_ = COBALT_VERSION;
}

std::unique_ptr<base::Value> ServerStatus::ToValue(const ServerStatus& status) {
  base::Value::Dict build_value;
  build_value.Set("time", status.build_time_);
  build_value.Set("version", status.build_version_);

  base::Value::Dict os_value;

  if (status.os_arch_) {
    os_value.Set("arch", *status.os_arch_);
  }
  if (status.os_name_) {
    os_value.Set("name", *status.os_name_);
  }
  if (status.os_version_) {
    os_value.Set("version", *status.os_version_);
  }

  base::Value ret(base::Value::Type::DICT);
  base::Value::Dict* status_value = ret->GetIfDict();
  status_value->Set("os", std::move(os_value));
  status_value->Set("build", std::move(build_value));

  return base::Value::ToUniquePtrValue(std::move(ret));
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
