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

#include <memory>

#include "cobalt/webdriver/protocol/server_status.h"

#include "cobalt/version.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

ServerStatus::ServerStatus() {
  const size_t kSystemPropertyMaxLength = 1024;
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
  std::unique_ptr<base::DictionaryValue> build_value(
      new base::DictionaryValue());
  build_value->SetString("time", status.build_time_);
  build_value->SetString("version", status.build_version_);

  std::unique_ptr<base::DictionaryValue> os_value(new base::DictionaryValue());

  if (status.os_arch_) {
    os_value->SetString("arch", *status.os_arch_);
  }
  if (status.os_name_) {
    os_value->SetString("name", *status.os_name_);
  }
  if (status.os_version_) {
    os_value->SetString("version", *status.os_version_);
  }

  std::unique_ptr<base::DictionaryValue> status_value(
      new base::DictionaryValue());
  status_value->Set("os", std::move(os_value));
  status_value->Set("build", std::move(build_value));

  return std::unique_ptr<base::Value>(status_value.release());
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
