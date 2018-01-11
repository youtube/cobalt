// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_INTERFACE_WITH_DATE_H_
#define COBALT_BINDINGS_TESTING_INTERFACE_WITH_DATE_H_

#include "base/compiler_specific.h"
#include "base/time.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace bindings {
namespace testing {

class InterfaceWithDate : public script::Wrappable {
 public:
  InterfaceWithDate() = default;

  const base::Time& GetDate() const { return date_; }

  void SetDate(const base::Time& date) { date_ = date; }

  DEFINE_WRAPPABLE_TYPE(InterfaceWithDate);

 private:
  base::Time date_ = base::Time();
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_INTERFACE_WITH_DATE_H_
