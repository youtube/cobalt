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

#include "cobalt/h5vcc/h5vcc_account_info.h"

#include <memory>
#include <string>

#include "starboard/user.h"

namespace cobalt {
namespace h5vcc {

namespace {
const int kMaxValueLength = 64 * 1024;

std::string GetCurrentUserProperty(SbUserPropertyId property_id) {
  SbUser user = SbUserGetCurrent();

  if (!SbUserIsValid(user)) {
    return "";
  }

  int size = SbUserGetPropertySize(user, property_id);
  if (!size || size > kMaxValueLength) {
    return "";
  }

  std::unique_ptr<char[]> value(new char[size]);
  if (!SbUserGetProperty(user, property_id, value.get(), size)) {
    return "";
  }

  std::string result = value.get();
  return result;
}

}  // namespace

H5vccAccountInfo::H5vccAccountInfo() {}

std::string H5vccAccountInfo::avatar_url() const {
  return GetCurrentUserProperty(kSbUserPropertyAvatarUrl);
}

std::string H5vccAccountInfo::username() const {
  return GetCurrentUserProperty(kSbUserPropertyUserName);
}

std::string H5vccAccountInfo::user_id() const {
  return GetCurrentUserProperty(kSbUserPropertyUserId);
}

}  // namespace h5vcc
}  // namespace cobalt
