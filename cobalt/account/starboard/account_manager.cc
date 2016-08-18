/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/account/account_manager.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/event_dispatcher.h"
#include "starboard/user.h"

namespace cobalt {
namespace account {

class AccountManagerStarboard : public AccountManager {
 public:
  explicit AccountManagerStarboard(base::EventDispatcher* event_dispatcher)
      : event_dispatcher_(event_dispatcher) {}
  ~AccountManagerStarboard() {}

  std::string GetAvatarURL() OVERRIDE;
  std::string GetUsername() OVERRIDE;
  std::string GetUserId() OVERRIDE;
  void StartSignIn() OVERRIDE;
  bool IsAgeRestricted() OVERRIDE;

 private:
  base::EventDispatcher* event_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(AccountManagerStarboard);
};

scoped_ptr<AccountManager> AccountManager::Create(
    base::EventDispatcher* event_dispatcher) {
  scoped_ptr<AccountManager> account_manager(
      new AccountManagerStarboard(event_dispatcher));
  return account_manager.Pass();
}

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

  scoped_array<char> value(new char[size]);
  if (!SbUserGetProperty(user, property_id, value.get(), size)) {
    return "";
  }

  std::string result = value.get();
  return result;
}

}  // namespace

std::string AccountManagerStarboard::GetAvatarURL() {
  return GetCurrentUserProperty(kSbUserPropertyAvatarUrl);
}

std::string AccountManagerStarboard::GetUsername() {
  return GetCurrentUserProperty(kSbUserPropertyUserName);
}

std::string AccountManagerStarboard::GetUserId() {
  return GetCurrentUserProperty(kSbUserPropertyUserId);
}

void AccountManagerStarboard::StartSignIn() {
  NOTREACHED() << "Should be handled internally by platform.";
}

bool AccountManagerStarboard::IsAgeRestricted() {
  NOTREACHED() << "Should be handled internally by platform.";
  return false;
}

}  // namespace account
}  // namespace cobalt
