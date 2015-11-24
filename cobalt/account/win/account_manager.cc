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

namespace cobalt {
namespace account {

// Static.
scoped_ptr<AccountManager> AccountManager::Create(
    base::EventDispatcher* event_dispatcher) {
  UNREFERENCED_PARAMETER(event_dispatcher);
  scoped_ptr<AccountManager> account_manager(new AccountManagerStub());
  return account_manager.Pass();
}

}  // namespace account
}  // namespace cobalt
