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

#ifndef COBALT_ACCOUNT_ACCOUNT_MANAGER_H_
#define COBALT_ACCOUNT_ACCOUNT_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/event_dispatcher.h"

namespace cobalt {
namespace account {

// Platform-specific user account management.
// The AccountManager will be owned by BrowserModule.
class AccountManager {
 public:
  virtual ~AccountManager() {}

  // To be implemented by each platform.
  // Platforms may wish to dispatch AccountEvents to any registered listeners.
  // Use the event_dispatcher for this.
  static scoped_ptr<AccountManager> Create(
      base::EventDispatcher* event_dispatcher);

  // Get the avatar URL associated with the account, if any.
  virtual std::string GetAvatarURL() = 0;

  // Get the username associated with the account. Due to restrictions on
  // some platforms, this may return the user ID or an empty string.
  virtual std::string GetUsername() = 0;

  // Get the user ID associated with the account.
  virtual std::string GetUserId() = 0;

  // Initiate user sign-in (e.g. open a sign-in dialog)
  virtual void StartSignIn() = 0;

  // Is access by the currently signed-in user restricted by age?
  virtual bool IsAgeRestricted() = 0;

 protected:
  AccountManager() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManager);
};

}  // namespace account
}  // namespace cobalt

#endif  // COBALT_ACCOUNT_ACCOUNT_MANAGER_H_
