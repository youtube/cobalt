// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_HISTORY_H_
#define COBALT_DOM_HISTORY_H_

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The History object can be used to maintain browser session history.
// https://www.w3.org/TR/html5/browsers.html#the-history-interface
// https://www.w3.org/TR/html5/browsers.html#history-1
// This implementation is extremely basic.
class History : public script::Wrappable {
 public:
  History();

  // Web API: History
  int length() const;

  DEFINE_WRAPPABLE_TYPE(History);

 private:
  ~History() override {}

  DISALLOW_COPY_AND_ASSIGN(History);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HISTORY_H_
