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

#ifndef COBALT_BINDINGS_TESTING_EXCEPTIONS_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_EXCEPTIONS_INTERFACE_H_

#include "base/lazy_instance.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class ExceptionsInterface : public script::Wrappable {
 public:
  class ConstructorImplementationMock {
   public:
    MOCK_METHOD1(Constructor, void(script::ExceptionState*));
  };
  ExceptionsInterface() {}
  explicit ExceptionsInterface(script::ExceptionState* exception_state) {
    constructor_implementation_mock.Get().Constructor(exception_state);
  }

  MOCK_METHOD1(FunctionThrowsException, void(script::ExceptionState*));
  MOCK_METHOD1(attribute_throws_exception, bool(script::ExceptionState*));
  MOCK_METHOD2(set_attribute_throws_exception,
               void(bool, script::ExceptionState*));

  DEFINE_WRAPPABLE_TYPE(ExceptionsInterface);

  static base::LazyInstance<
      ::testing::StrictMock<ConstructorImplementationMock> >::DestructorAtExit
      constructor_implementation_mock;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_EXCEPTIONS_INTERFACE_H_
