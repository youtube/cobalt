// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/template_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

struct AStruct {};
class AClass {};
enum AnEnum {};

class Parent {};
class Child : public Parent {};

TEST(TemplateUtilTest, IsPointer) {
  EXPECT_FALSE(is_pointer<int>::value);
  EXPECT_FALSE(is_pointer<int&>::value);
  EXPECT_TRUE(is_pointer<int*>::value);
  EXPECT_TRUE(is_pointer<const int*>::value);
}

TEST(TemplateUtilTest, IsArray) {
  EXPECT_FALSE(is_array<int>::value);
  EXPECT_FALSE(is_array<int*>::value);
  EXPECT_FALSE(is_array<int(*)[3]>::value);
  EXPECT_TRUE(is_array<int[]>::value);
  EXPECT_TRUE(is_array<const int[]>::value);
  EXPECT_TRUE(is_array<int[3]>::value);
}

TEST(TemplateUtilTest, IsNonConstReference) {
  EXPECT_FALSE(is_non_const_reference<int>::value);
  EXPECT_FALSE(is_non_const_reference<const int&>::value);
  EXPECT_TRUE(is_non_const_reference<int&>::value);
}

TEST(TemplateUtilTest, IsConvertible) {
  // Extra parens needed to make EXPECT_*'s parsing happy. Otherwise,
  // it sees the equivalent of
  //
  //  EXPECT_TRUE( (is_convertible < Child), (Parent > ::value));
  //
  // Silly C++.
  EXPECT_TRUE( (is_convertible<Child, Parent>::value) );
  EXPECT_FALSE( (is_convertible<Parent, Child>::value) );
  EXPECT_FALSE( (is_convertible<Parent, AStruct>::value) );

  EXPECT_TRUE( (is_convertible<int, double>::value) );
  EXPECT_TRUE( (is_convertible<int*, void*>::value) );
  EXPECT_FALSE( (is_convertible<void*, int*>::value) );
}

TEST(TemplateUtilTest, IsClass) {
  EXPECT_TRUE(is_class<AStruct>::value);
  EXPECT_TRUE(is_class<AClass>::value);

  EXPECT_FALSE(is_class<AnEnum>::value);
  EXPECT_FALSE(is_class<int>::value);
  EXPECT_FALSE(is_class<char*>::value);
  EXPECT_FALSE(is_class<int&>::value);
  EXPECT_FALSE(is_class<char[3]>::value);
}

}  // namespace
}  // namespace base
