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

#ifndef COBALT_BINDINGS_TESTING_STATIC_PROPERTIES_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_STATIC_PROPERTIES_INTERFACE_H_

#include <string>

#include "base/lazy_instance.h"
#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class StaticPropertiesInterface : public script::Wrappable {
 public:
  class StaticMethodsMock {
   public:
    MOCK_METHOD0(StaticFunction, void());
    MOCK_METHOD1(StaticFunction, void(int32_t));
    MOCK_METHOD1(StaticFunction, void(const std::string&));
    MOCK_METHOD3(StaticFunction, void(int32_t, int32_t, int32_t));
    MOCK_METHOD3(StaticFunction,
                 void(int32_t, int32_t,
                      const scoped_refptr<ArbitraryInterface>&));
    MOCK_METHOD0(static_attribute, std::string());
    MOCK_METHOD1(set_static_attribute, void(const std::string&));
  };

  static void StaticFunction() { static_methods_mock.Get().StaticFunction(); }
  static void StaticFunction(int32_t arg) {
    static_methods_mock.Get().StaticFunction(arg);
  }
  static void StaticFunction(const std::string& arg) {
    static_methods_mock.Get().StaticFunction(arg);
  }
  static void StaticFunction(int32_t arg1, int32_t arg2, int32_t arg3) {
    static_methods_mock.Get().StaticFunction(arg1, arg2, arg3);
  }
  static void StaticFunction(int32_t arg1, int32_t arg2,
                             const scoped_refptr<ArbitraryInterface>& arg3) {
    static_methods_mock.Get().StaticFunction(arg1, arg2, arg3);
  }
  static std::string static_attribute() {
    return static_methods_mock.Get().static_attribute();
  }
  static void set_static_attribute(const std::string& arg) {
    static_methods_mock.Get().set_static_attribute(arg);
  }

  DEFINE_WRAPPABLE_TYPE(StaticPropertiesInterface);

  static base::LazyInstance< ::testing::StrictMock<StaticMethodsMock> >::
      DestructorAtExit static_methods_mock;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_STATIC_PROPERTIES_INTERFACE_H_
