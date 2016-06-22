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

#include <limits>

#include "cobalt/base/c_val.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace base {

TEST(CValTest, GetCValManagerInstance) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
}

TEST(CValTest, InitiallyEmpty) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  std::set<std::string> cval_names = cvm->GetOrderedCValNames();
  std::set<std::string>::size_type size = cval_names.size();
  EXPECT_EQ(size, 0);
}

TEST(CValTest, RegisterAndUnregisterCVal) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  std::set<std::string> cval_names = cvm->GetOrderedCValNames();
  std::set<std::string>::size_type size = cval_names.size();
  EXPECT_EQ(size, 0);
  {
    base::DebugCVal<int32_t> cval("S32", 0, "Test.");
    cval_names = cvm->GetOrderedCValNames();
    size = cval_names.size();
    EXPECT_EQ(size, 1);
  }
  cval_names = cvm->GetOrderedCValNames();
  size = cval_names.size();
  EXPECT_EQ(size, 0);
}

TEST(CValTest, RegisterAndPrintU32) {
  const std::string cval_name = "32-bit unsigned int";
  const uint32_t cval_value = std::numeric_limits<uint32_t>::max();
  base::DebugCVal<uint32_t> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "4095MB");
}

TEST(CValTest, RegisterAndPrintU64) {
  const std::string cval_name = "64-bit unsigned int";
  const uint64_t cval_value = std::numeric_limits<uint64_t>::max();
  base::DebugCVal<uint64_t> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "17592186044415MB");
}

TEST(CValTest, RegisterAndPrintS32) {
  const std::string cval_name = "32-bit signed int";
  const int32_t cval_value = std::numeric_limits<int32_t>::min();
  base::DebugCVal<int32_t> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "-2147483648");
}

TEST(CValTest, RegisterAndPrintS64) {
  const std::string cval_name = "64-bit signed int";
  const int64_t cval_value = std::numeric_limits<int64_t>::min();
  base::DebugCVal<int64_t> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "-9223372036854775808");
}

TEST(CValTest, RegisterAndPrintFloat) {
  const std::string cval_name = "float";
  const float cval_value = 3.14159f;
  base::DebugCVal<float> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "3.14159");
}

TEST(CValTest, RegisterAndPrintDouble) {
  const std::string cval_name = "double";
  const double cval_value = 3.14159;
  base::DebugCVal<double> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "3.14159");
}

TEST(CValTest, RegisterAndPrintString) {
  const std::string cval_name = "string";
  const std::string cval_value = "Hello Cobalt!";
  base::DebugCVal<std::string> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, cval_value);
}

TEST(CValTest, GetOrderedCValNames) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  std::set<std::string> cval_names = cvm->GetOrderedCValNames();
  std::set<std::string>::size_type size = cval_names.size();
  EXPECT_EQ(size, 0);

  const int num_cvals = 4;
  std::string names[num_cvals] = {"a", "aa", "b", "c"};
  base::DebugCVal<int32_t> cval3(names[3], 0, "Test.");
  base::DebugCVal<int32_t> cval1(names[1], 0, "Test.");
  base::PublicCVal<int32_t> cval0(names[0], 0, "Test.");
  base::PublicCVal<int32_t> cval2(names[2], 0, "Test.");

  cval_names = cvm->GetOrderedCValNames();
  size = cval_names.size();
  EXPECT_EQ(size, num_cvals);

  int i = 0;
  for (std::set<std::string>::const_iterator it = cval_names.begin();
       it != cval_names.end(); ++it, ++i) {
    const std::string name = *it;
    EXPECT_EQ(name, names[i]);
  }
}

TEST(CValTest, ValueChangedCallback) {
  class TestOnChangedHook : public base::CValManager::OnChangedHook {
   public:
    TestOnChangedHook() : got_callback_(false) {}
    void OnValueChanged(const std::string& name,
                        const base::CValGenericValue& value) OVERRIDE {
      EXPECT_EQ(name, "S32");
      EXPECT_EQ(value.GetType(), base::kS32);
      EXPECT_EQ(value.IsNativeType<int32_t>(), true);
      EXPECT_EQ(value.AsNativeType<int32_t>(), 1);
      EXPECT_EQ(value.AsString(), "1");
      got_callback_ = true;
    }
    bool got_callback_;
  };

  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  base::DebugCVal<int32_t> cval("S32", 0, "Test.");
  TestOnChangedHook on_changed_hook;
  EXPECT_EQ(on_changed_hook.got_callback_, false);
  cval = 1;
  EXPECT_EQ(on_changed_hook.got_callback_, true);
}

TEST(CValTest, HookReceiver) {
  base::DebugCVal<int32_t> cval_a("cval_a", 10, "");

  {
    class TestHook : public base::CValManager::OnChangedHook {
     public:
      TestHook() : value_changed_count_(0) {}

      virtual void OnValueChanged(const std::string& name,
                                  const base::CValGenericValue& value) {
        EXPECT_EQ(name, "cval_a");
        EXPECT_EQ(value.AsString(), "15");

        ++value_changed_count_;
      }

      int value_changed_count_;
    };
    TestHook test_hook;

    cval_a = 15;

    EXPECT_EQ(test_hook.value_changed_count_, 1);
  }

  // Make sure there's no interference from the previous hook which should
  // be completely non-existant now
  {
    class TestHook : public base::CValManager::OnChangedHook {
     public:
      TestHook() : value_changed_count_(0) {}

      virtual void OnValueChanged(const std::string& name,
                                  const base::CValGenericValue& value) {
        EXPECT_EQ(name, "cval_a");
        EXPECT_EQ(value.AsString(), "20");

        ++value_changed_count_;
      }

      int value_changed_count_;
    };
    TestHook test_hook;

    cval_a = 20;

    EXPECT_EQ(test_hook.value_changed_count_, 1);
  }

  // Try modifying a new variable with a different name.  Also checks that
  // past hooks will not be checked.
  {
    class TestHook : public base::CValManager::OnChangedHook {
     public:
      TestHook() : value_changed_count_(0) {}

      virtual void OnValueChanged(const std::string& name,
                                  const base::CValGenericValue& value) {
        EXPECT_EQ(name, "cval_b");
        EXPECT_EQ(value.AsString(), "bar");

        ++value_changed_count_;
      }

      int value_changed_count_;
    };
    TestHook test_hook;

    base::DebugCVal<std::string> cval_b("cval_b", "foo", "");
    cval_b = "bar";

    EXPECT_EQ(test_hook.value_changed_count_, 1);
  }
}

TEST(CValTest, NativeType) {
  class TestInt32Hook : public base::CValManager::OnChangedHook {
   public:
    TestInt32Hook() : value_changed_count_(0) {}

    virtual void OnValueChanged(const std::string& name,
                                const base::CValGenericValue& value) {
      UNREFERENCED_PARAMETER(name);
      EXPECT_TRUE(value.IsNativeType<int32_t>());

      EXPECT_FALSE(value.IsNativeType<uint32_t>());
      EXPECT_FALSE(value.IsNativeType<int64_t>());
      EXPECT_FALSE(value.IsNativeType<uint64_t>());
      EXPECT_FALSE(value.IsNativeType<float>());
      EXPECT_FALSE(value.IsNativeType<double>());
      EXPECT_FALSE(value.IsNativeType<std::string>());

      EXPECT_TRUE(value.IsNativeType<int32_t>());
      EXPECT_EQ(value.AsNativeType<int32_t>(), 15);

      ++value_changed_count_;
    }

    int value_changed_count_;
  };
  TestInt32Hook test_hook;

  base::DebugCVal<int32_t> cv_int32_t("cv_int32_t", 10, "");
  cv_int32_t = 15;

  EXPECT_EQ(test_hook.value_changed_count_, 1);
}

}  // namespace base
