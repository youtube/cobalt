// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

const wchar_t kRootKey[] = L"Base_Registry_Unittest";

class RegistryTest : public testing::Test {
 public:
  RegistryTest() {}

 protected:
  virtual void SetUp() {
    // Create a temporary key.
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    key.DeleteKey(kRootKey);
    ASSERT_FALSE(key.Open(HKEY_CURRENT_USER, kRootKey, KEY_READ));
    ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, kRootKey, KEY_READ));
  }

  virtual void TearDown() {
    // Clean up the temporary key.
    RegKey key(HKEY_CURRENT_USER, L"", KEY_SET_VALUE);
    ASSERT_TRUE(key.DeleteKey(kRootKey));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RegistryTest);
};

TEST_F(RegistryTest, ValueTest) {
  RegKey key;

  std::wstring foo_key(kRootKey);
  foo_key += L"\\Foo";
  ASSERT_TRUE(key.Create(HKEY_CURRENT_USER, foo_key.c_str(), KEY_READ));

  {
    ASSERT_TRUE(key.Open(HKEY_CURRENT_USER, foo_key.c_str(),
                         KEY_READ | KEY_SET_VALUE));

    const wchar_t* kName = L"Bar";
    const wchar_t* kValue = L"bar";
    EXPECT_TRUE(key.WriteValue(kName, kValue));
    EXPECT_TRUE(key.ValueExists(kName));
    std::wstring out_value;
    EXPECT_TRUE(key.ReadValue(kName, &out_value));
    EXPECT_NE(out_value, L"");
    EXPECT_STREQ(out_value.c_str(), kValue);
    EXPECT_EQ(1U, key.ValueCount());
    EXPECT_TRUE(key.DeleteValue(kName));
  }
}

}  // namespace

}  // namespace win
}  // namespace base
