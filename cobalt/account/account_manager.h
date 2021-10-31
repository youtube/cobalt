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

#ifndef COBALT_ACCOUNT_ACCOUNT_MANAGER_H_
#define COBALT_ACCOUNT_ACCOUNT_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "cobalt/base/event_dispatcher.h"

namespace cobalt {
namespace account {

// Glue for h5vcc to get properties for the current user.
// The AccountManager will be owned by BrowserModule.
class AccountManager {
 public:
  AccountManager();

  ~AccountManager() {}

  // Get the avatar URL associated with the account, if any.
  std::string GetAvatarURL();

  // Get the username associated with the account. Due to restrictions on
  // some platforms, this may return the user ID or an empty string.
  std::string GetUsername();

  // Get the user ID associated with the account.
  std::string GetUserId();

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManager);
};

}  // namespace account
}  // namespace cobalt

#endif  // COBALT_ACCOUNT_ACCOUNT_MANAGER_H_
