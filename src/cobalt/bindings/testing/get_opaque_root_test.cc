/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/get_opaque_root_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
class GetOpaqueRootInterfaceTest : public BindingsTestBase {
 public:
  GetOpaqueRootInterfaceTest()
      : test_mock_(new ::testing::NiceMock<GetOpaqueRootInterface>()) {
    global_environment_->Bind(
        "test", make_scoped_refptr<GetOpaqueRootInterface>((test_mock_)));
  }

  GetOpaqueRootInterface& test_mock() { return *test_mock_.get(); }

 private:
  const scoped_refptr<GetOpaqueRootInterface> test_mock_;
};
}  // namespace

TEST_F(GetOpaqueRootInterfaceTest, CallsFunction) {
  EXPECT_CALL(test_mock(), add_opaque_root_function_name())
      .Times(::testing::AtLeast(1));
  CollectGarbage();
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
