// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/account/account_manager.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "cobalt/base/event_dispatcher.h"
#include "starboard/user.h"

namespace cobalt {
namespace account {

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

AccountManager::AccountManager() {}

std::string AccountManager::GetAvatarURL() {
  return GetCurrentUserProperty(kSbUserPropertyAvatarUrl);
}

std::string AccountManager::GetUsername() {
  return GetCurrentUserProperty(kSbUserPropertyUserName);
}

std::string AccountManager::GetUserId() {
  return GetCurrentUserProperty(kSbUserPropertyUserId);
}

}  // namespace account
}  // namespace cobalt
