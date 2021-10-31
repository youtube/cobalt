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

#ifndef COBALT_BINDINGS_TESTING_EXTENDED_IDL_ATTRIBUTES_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_EXTENDED_IDL_ATTRIBUTES_INTERFACE_H_

#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class ExtendedIDLAttributesInterface : public script::Wrappable {
 public:
  MOCK_METHOD1(CallWithSettings, void(script::EnvironmentSettings*));
  MOCK_METHOD1(ClampArgument, void(uint16_t));
  MOCK_METHOD0(attribute_default, bool());
  MOCK_METHOD1(set_attribute_default, void(bool));

  DEFINE_WRAPPABLE_TYPE(ExtendedIDLAttributesInterface);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_EXTENDED_IDL_ATTRIBUTES_INTERFACE_H_
