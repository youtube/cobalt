/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/webdriver/protocol/server_status.h"

// TODO: Support running WebDriver on platforms other than Linux.
#include <sys/utsname.h>

#include "cobalt/version.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

ServerStatus::ServerStatus() {
  struct utsname name_buffer;
  if (uname(&name_buffer) == 0) {
    os_name_ = name_buffer.sysname;
    os_version_ = name_buffer.version;
    os_arch_ = name_buffer.machine;
  }
  build_time_ = __DATE__ " (" __TIME__ ")";
  build_version_ = COBALT_VERSION;
}

scoped_ptr<base::Value> ServerStatus::ToValue(const ServerStatus& status) {
  scoped_ptr<base::DictionaryValue> build_value(new base::DictionaryValue());
  build_value->SetString("time", status.build_time_);
  build_value->SetString("version", status.build_version_);

  scoped_ptr<base::DictionaryValue> os_value(new base::DictionaryValue());
  os_value->SetString("arch", status.os_arch_);
  os_value->SetString("name", status.os_name_);
  os_value->SetString("version", status.os_version_);

  scoped_ptr<base::DictionaryValue> status_value(new base::DictionaryValue());
  status_value->Set("os", os_value.release());
  status_value->Set("build", build_value.release());

  return status_value.PassAs<base::Value>();
}

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
