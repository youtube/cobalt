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

#include "cobalt/bindings/testing/anonymous_getter_setter_interface.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/derived_getter_setter_interface.h"
#include "cobalt/bindings/testing/getter_setter_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::ReturnArg;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<GetterSetterInterface> GetterSetterBindingsTest;
typedef InterfaceBindingsTest<AnonymousGetterSetterInterface>
    AnonymousGetterSetterBindingsTest;
typedef InterfaceBindingsTest<DerivedGetterSetterInterface>
    DerivedGetterSetterBindingsTest;
}  // namespace

TEST_F(GetterSetterBindingsTest, IndexedGetter) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), IndexedGetter(_)).WillOnce(ReturnArg<0>());
  EXPECT_TRUE(EvaluateScript("test[4];", &result));
  EXPECT_STREQ("4", result.c_str());

  EXPECT_CALL(test_mock(), IndexedGetter(_)).WillOnce(ReturnArg<0>());
  EXPECT_TRUE(EvaluateScript("test.indexedGetter(6);", &result));
  EXPECT_STREQ("6", result.c_str());
}

TEST_F(GetterSetterBindingsTest, IndexedGetterOutOfRange) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_TRUE(EvaluateScript("test[20];", &result));
  EXPECT_STREQ("undefined", result.c_str());

  EXPECT_CALL(test_mock(), IndexedGetter(_)).WillOnce(ReturnArg<0>());
  EXPECT_TRUE(EvaluateScript("test.indexedGetter(20);", &result));
  EXPECT_STREQ("20", result.c_str());
}

TEST_F(GetterSetterBindingsTest, IndexedSetter) {
  InSequence dummy;

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), IndexedSetter(4, 100));
  EXPECT_TRUE(EvaluateScript("test[4] = 100;", NULL));

  EXPECT_CALL(test_mock(), IndexedSetter(4, 100));
  EXPECT_TRUE(EvaluateScript("test.indexedSetter(4, 100);", NULL));
}

TEST_F(GetterSetterBindingsTest, IndexedSetterOutOfRange) {
  InSequence dummy;

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(1));
  EXPECT_CALL(test_mock(), IndexedSetter(_, _)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[4] = 100;", NULL));

  EXPECT_CALL(test_mock(), IndexedSetter(4, 100));
  EXPECT_TRUE(EvaluateScript("test.indexedSetter(4, 100);", NULL));
}

TEST_F(AnonymousGetterSetterBindingsTest, IndexedGetter) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), AnonymousIndexedGetter(_)).WillOnce(ReturnArg<0>());
  EXPECT_TRUE(EvaluateScript("test[4];", &result));
  EXPECT_STREQ("4", result.c_str());

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), AnonymousIndexedGetter(_)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[10];", NULL));
}

TEST_F(AnonymousGetterSetterBindingsTest, IndexedSetter) {
  InSequence dummy;

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), AnonymousIndexedSetter(4, 100));
  EXPECT_TRUE(EvaluateScript("test[4] = 100;", NULL));

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), AnonymousIndexedSetter(_, _)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[10] = 100;", NULL));
}

TEST_F(DerivedGetterSetterBindingsTest, OverridesGetterAndSetter) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), DerivedIndexedGetter(4)).WillOnce(Return(100));
  EXPECT_CALL(test_mock(), IndexedGetter(_)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[4] == 100;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), length()).WillOnce(Return(10));
  EXPECT_CALL(test_mock(), DerivedIndexedSetter(4, 100));
  EXPECT_CALL(test_mock(), IndexedSetter(_, _)).Times(0);
  EXPECT_TRUE(EvaluateScript("test[4] = 100;", NULL));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
