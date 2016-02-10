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

#ifndef COBALT_ACCOUNT_ACCOUNT_MANAGER_STUB_H_
#define COBALT_ACCOUNT_ACCOUNT_MANAGER_STUB_H_

#include "cobalt/account/account_manager.h"

#include <string>

namespace cobalt {
namespace account {

class AccountManagerStub : public AccountManager {
 public:
  AccountManagerStub();
  ~AccountManagerStub() OVERRIDE;
  std::string GetAvatarURL() OVERRIDE;
  std::string GetUsername() OVERRIDE;
  std::string GetUserId() OVERRIDE;
  void StartSignIn() OVERRIDE;
  bool IsAgeRestricted() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagerStub);
};

}  // namespace account
}  // namespace cobalt

#endif  // COBALT_ACCOUNT_ACCOUNT_MANAGER_STUB_H_
