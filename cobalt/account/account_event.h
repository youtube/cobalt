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

#ifndef COBALT_ACCOUNT_ACCOUNT_EVENT_H_
#define COBALT_ACCOUNT_ACCOUNT_EVENT_H_

#include "base/compiler_specific.h"
#include "cobalt/base/event.h"

namespace cobalt {
namespace account {

class AccountEvent : public base::Event {
 public:
  enum Type { kSignedIn, kSignedOut };

  explicit AccountEvent(Type type) : type_(type) {}
  Type type() const { return type_; }

  BASE_EVENT_SUBCLASS(AccountEvent);

 private:
  Type type_;
};

}  // namespace account
}  // namespace cobalt

#endif  // COBALT_ACCOUNT_ACCOUNT_EVENT_H_
