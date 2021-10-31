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

#include "base/memory/ref_counted.h"
#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/nested_put_forwards_interface.h"
#include "cobalt/bindings/testing/put_forwards_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::StrictMock;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<PutForwardsInterface> PutForwardsTest;
typedef InterfaceBindingsTest<NestedPutForwardsInterface> NestedPutForwardsTest;
}  // namespace

TEST_F(PutForwardsTest, ForwardsToInterface) {
  scoped_refptr<StrictMock<ArbitraryInterface> > arbitrary_interface_mock(
      new StrictMock<ArbitraryInterface>());
  EXPECT_CALL(test_mock(), forwarding_attribute())
      .WillOnce(Return(arbitrary_interface_mock));
  EXPECT_CALL(*arbitrary_interface_mock, set_arbitrary_property("apple"));
  EXPECT_TRUE(EvaluateScript("test.forwardingAttribute = 'apple';", NULL));
}

TEST_F(PutForwardsTest, StaticForwardsToInterface) {
  scoped_refptr<StrictMock<ArbitraryInterface> > arbitrary_interface_mock(
      new StrictMock<ArbitraryInterface>());
  EXPECT_CALL(PutForwardsInterface::static_methods_mock.Get(),
              static_forwarding_attribute())
      .WillOnce(Return(arbitrary_interface_mock));
  EXPECT_CALL(*arbitrary_interface_mock, set_arbitrary_property("orange"));
  EXPECT_TRUE(EvaluateScript(
      "PutForwardsInterface.staticForwardingAttribute = 'orange';", NULL));
}

TEST_F(NestedPutForwardsTest, ForwardsToNestedInterface) {
  scoped_refptr<StrictMock<PutForwardsInterface> > puts_forward_interface_mock(
      new StrictMock<PutForwardsInterface>());
  scoped_refptr<StrictMock<ArbitraryInterface> > arbitrary_interface_mock(
      new StrictMock<ArbitraryInterface>());
  EXPECT_CALL(test_mock(), nested_forwarding_attribute())
      .WillOnce(Return(puts_forward_interface_mock));
  EXPECT_CALL(*puts_forward_interface_mock, forwarding_attribute())
      .WillOnce(Return(arbitrary_interface_mock));
  EXPECT_CALL(*arbitrary_interface_mock, set_arbitrary_property("banana"));
  EXPECT_TRUE(
      EvaluateScript("test.nestedForwardingAttribute = 'banana';", NULL));
}
}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
