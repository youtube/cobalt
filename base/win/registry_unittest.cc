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
    ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, kRootKey, KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, kRootKey, KEY_READ));
  }

  virtual void TearDown() {
    // Clean up the temporary key.
    RegKey key(HKEY_CURRENT_USER, L"", KEY_SET_VALUE);
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(kRootKey));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RegistryTest);
};

TEST_F(RegistryTest, ValueTest) {
  RegKey key;

  std::wstring foo_key(kRootKey);
  foo_key += L"\\Foo";
  ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, foo_key.c_str(),
                                      KEY_READ));

  {
    ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, foo_key.c_str(),
                                      KEY_READ | KEY_SET_VALUE));
    ASSERT_TRUE(key.Valid());

    const wchar_t* kStringValueName = L"StringValue";
    const wchar_t* kDWORDValueName = L"DWORDValue";
    const wchar_t* kInt64ValueName = L"Int64Value";
    const wchar_t* kStringData = L"string data";
    const DWORD kDWORDData = 0xdeadbabe;
    const int64 kInt64Data = 0xdeadbabedeadbabeLL;

    // Test value creation
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kStringValueName, kStringData));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kDWORDValueName, kDWORDData));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kInt64ValueName, &kInt64Data,
                                            sizeof(kInt64Data), REG_QWORD));
    EXPECT_EQ(3U, key.ValueCount());
    EXPECT_TRUE(key.ValueExists(kStringValueName));
    EXPECT_TRUE(key.ValueExists(kDWORDValueName));
    EXPECT_TRUE(key.ValueExists(kInt64ValueName));

    // Test Read
    std::wstring string_value;
    DWORD dword_value = 0;
    int64 int64_value = 0;
    ASSERT_EQ(ERROR_SUCCESS, key.ReadValue(kStringValueName, &string_value));
    ASSERT_EQ(ERROR_SUCCESS, key.ReadValueDW(kDWORDValueName, &dword_value));
    ASSERT_EQ(ERROR_SUCCESS, key.ReadInt64(kInt64ValueName, &int64_value));
    EXPECT_STREQ(kStringData, string_value.c_str());
    EXPECT_EQ(kDWORDData, dword_value);
    EXPECT_EQ(kInt64Data, int64_value);

    // Make sure out args are not touched if ReadValue fails
    const wchar_t* kNonExistent = L"NonExistent";
    ASSERT_NE(ERROR_SUCCESS, key.ReadValue(kNonExistent, &string_value));
    ASSERT_NE(ERROR_SUCCESS, key.ReadValueDW(kNonExistent, &dword_value));
    ASSERT_NE(ERROR_SUCCESS, key.ReadInt64(kNonExistent, &int64_value));
    EXPECT_STREQ(kStringData, string_value.c_str());
    EXPECT_EQ(kDWORDData, dword_value);
    EXPECT_EQ(kInt64Data, int64_value);

    // Test delete
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kStringValueName));
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kDWORDValueName));
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kInt64ValueName));
    EXPECT_EQ(0U, key.ValueCount());
    EXPECT_FALSE(key.ValueExists(kStringValueName));
    EXPECT_FALSE(key.ValueExists(kDWORDValueName));
    EXPECT_FALSE(key.ValueExists(kInt64ValueName));
  }
}

}  // namespace

}  // namespace win
}  // namespace base
