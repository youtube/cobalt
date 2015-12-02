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

#include "cobalt/account/account_manager_stub.h"

#include <string>

namespace cobalt {
namespace account {

AccountManagerStub::AccountManagerStub() {}

AccountManagerStub::~AccountManagerStub() {}

std::string AccountManagerStub::GetAvatarURL() { return ""; }

std::string AccountManagerStub::GetUsername() { return ""; }

std::string AccountManagerStub::GetUserId() { return ""; }

void AccountManagerStub::StartSignIn() {}

bool AccountManagerStub::IsAgeRestricted() { return false; }

}  // namespace account
}  // namespace cobalt
