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

#ifndef COBALT_BINDINGS_TESTING_CONSTRUCTOR_WITH_ARGUMENTS_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_CONSTRUCTOR_WITH_ARGUMENTS_INTERFACE_H_

#include <string>

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace bindings {
namespace testing {

class ConstructorWithArgumentsInterface : public script::Wrappable {
 public:
  ConstructorWithArgumentsInterface() {}
  ConstructorWithArgumentsInterface(int32_t arg1, bool arg2,
                                    const std::string& arg3)
      : arg1_(arg1), arg2_(arg2), arg3_(arg3) {}

  int32_t long_arg() { return arg1_; }
  bool boolean_arg() { return arg2_; }
  std::string string_arg() { return arg3_; }

  DEFINE_WRAPPABLE_TYPE(ConstructorWithArgumentsInterface);

 private:
  int32_t arg1_;
  bool arg2_;
  std::string arg3_;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_CONSTRUCTOR_WITH_ARGUMENTS_INTERFACE_H_
