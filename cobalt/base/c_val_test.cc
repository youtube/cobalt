// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <limits>
#include <utility>

#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "base/time.h"
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
    base::CVal<int32_t> cval("S32", 0, "Test.");
    cval_names = cvm->GetOrderedCValNames();
    size = cval_names.size();
    EXPECT_EQ(size, 1);
  }
  cval_names = cvm->GetOrderedCValNames();
  size = cval_names.size();
  EXPECT_EQ(size, 0);
}

TEST(CValTest, RegisterAndPrintBoolTrue) {
  const std::string cval_name = "Bool value";
  const bool cval_value = true;
  base::CVal<bool> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "true");
}

TEST(CValTest, RegisterAndPrintBoolFalse) {
  const std::string cval_name = "Bool value";
  const bool cval_value = false;
  base::CVal<bool> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "false");
}

TEST(CValTest, RegisterAndPrintU32) {
  const std::string cval_name = "32-bit unsigned int";
  const uint32_t cval_value = std::numeric_limits<uint32_t>::max();
  base::CVal<uint32_t> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "4294M");
}

TEST(CValTest, RegisterAndPrintU32Zero) {
  const std::string cval_name = "32-bit unsigned int";
  const uint32_t cval_value = 0;
  base::CVal<uint32_t> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "0");
}

TEST(CValTest, RegisterAndPrintU32K) {
  const std::string cval_name = "32-bit unsigned int";
  const uint32_t cval_value = 50000;
  base::CVal<uint32_t> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "50K");
}

TEST(CValTest, RegisterAndPrintU64) {
  const std::string cval_name = "64-bit unsigned int";
  const uint64_t cval_value = std::numeric_limits<uint64_t>::max();
  base::CVal<uint64_t> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "18446744073709M");
}

TEST(CValTest, RegisterAndPrintS32) {
  const std::string cval_name = "32-bit signed int";
  const int32_t cval_value = std::numeric_limits<int32_t>::min();
  base::CVal<int32_t> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "-2147483648");
}

TEST(CValTest, RegisterAndPrintS64) {
  const std::string cval_name = "64-bit signed int";
  const int64_t cval_value = std::numeric_limits<int64_t>::min();
  base::CVal<int64_t> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "-9223372036854775808");
}

TEST(CValTest, RegisterAndPrintSizeInBytesB) {
  const std::string cval_name = "SizeInBytes";
  cval::SizeInBytes cval_value(500);
  base::CVal<cval::SizeInBytes> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "500B");
}

TEST(CValTest, RegisterAndPrintSizeInBytesZero) {
  const std::string cval_name = "SizeInBytes";
  cval::SizeInBytes cval_value(0);
  base::CVal<cval::SizeInBytes> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "0B");
}

TEST(CValTest, RegisterAndPrintSizeInBytesKB) {
  const std::string cval_name = "SizeInBytes";
  cval::SizeInBytes cval_value(50000UL);
  base::CVal<cval::SizeInBytes> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "48KB");
}

TEST(CValTest, RegisterAndPrintSizeInBytesMB) {
  const std::string cval_name = "SizeInBytes";
  cval::SizeInBytes cval_value(50000000UL);
  base::CVal<cval::SizeInBytes> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "47MB");
}

TEST(CValTest, RegisterAndPrintSizeInBytesGB) {
  const std::string cval_name = "SizeInBytes";
  cval::SizeInBytes cval_value(50000000000UL);
  base::CVal<cval::SizeInBytes> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "46GB");
}

TEST(CValTest, RegisterAndPrintTimeDeltaMicroseconds) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMicroseconds(50));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "50us");
}

TEST(CValTest, RegisterAndPrintTimeDeltaZero) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMicroseconds(0));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "0us");
}

TEST(CValTest, RegisterAndPrintTimeDeltaMilliseconds) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMilliseconds(50));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "50.0ms");
}

TEST(CValTest, RegisterAndPrintTimeDeltaMillisecondsSingleDigit) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMilliseconds(5));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "5.0ms");
}

TEST(CValTest, RegisterAndPrintTimeDeltaMillisecondsFraction) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMicroseconds(5500));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "5.5ms");
}

TEST(CValTest, RegisterAndPrintTimeDeltaSeconds) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromSeconds(50));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "50.0s");
}

TEST(CValTest, RegisterAndPrintTimeDeltaSecondsSingleDigit) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromSeconds(5));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "5.0s");
}

TEST(CValTest, RegisterAndPrintTimeDeltaSecondsFraction) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMilliseconds(5500));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "5.5s");
}

TEST(CValTest, RegisterAndPrintTimeDeltaMinutes) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMinutes(50));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "50:00m");
}

TEST(CValTest, RegisterAndPrintTimeDeltaMinutesSingleDigit) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMinutes(5));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "5:00m");
}

TEST(CValTest, RegisterAndPrintTimeDeltaMinutesAndSeconds) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromSeconds(92));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "1:32m");
}

TEST(CValTest, RegisterAndPrintTimeDeltaHours) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromHours(50));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "50:00:00h");
}

TEST(CValTest, RegisterAndPrintTimeDeltaHoursSingleDigit) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromHours(5));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "5:00:00h");
}

TEST(CValTest, RegisterAndPrintTimeDeltaHoursAndMinutes) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMinutes(92));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "1:32:00h");
}

TEST(CValTest, RegisterAndPrintTimeDeltaHoursAndMinutesAndSeconds) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromSeconds(92 * 60 + 32));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "1:32:32h");
}

TEST(CValTest, RegisterAndPrintTimeDeltaHoursAndSeconds) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromSeconds(60 * 60 + 32));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "1:00:32h");
}

TEST(CValTest, RegisterAndPrintTimeDeltaNegativeMicroseconds) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMicroseconds(-3));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "-3us");
}

TEST(CValTest, RegisterAndPrintTimeDeltaNegativeMilliseconds) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMilliseconds(-3));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "-3.0ms");
}

TEST(CValTest, RegisterAndPrintTimeDeltaNegativeSeconds) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromSeconds(-3));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "-3.0s");
}

TEST(CValTest, RegisterAndPrintTimeDeltaNegativeMinutes) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromMinutes(-3));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "-3:00m");
}

TEST(CValTest, RegisterAndPrintTimeDeltaNegativeHours) {
  const std::string cval_name = "TimeDelta";
  base::TimeDelta cval_value(base::TimeDelta::FromHours(-3));
  base::CVal<base::TimeDelta> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "-3:00:00h");
}

TEST(CValTest, RegisterAndPrintFloat) {
  const std::string cval_name = "float";
  const float cval_value = 3.14159f;
  base::CVal<float> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "3.14159");
}

TEST(CValTest, RegisterAndPrintDouble) {
  const std::string cval_name = "double";
  const double cval_value = 3.14159;
  base::CVal<double> cval(cval_name, cval_value, "Description.");
  base::CValManager* cvm = base::CValManager::GetInstance();
  base::optional<std::string> result = cvm->GetValueAsPrettyString(cval_name);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "3.14159");
}

TEST(CValTest, RegisterAndPrintString) {
  const std::string cval_name = "string";
  const std::string cval_value = "Hello Cobalt!";
  base::CVal<std::string> cval(cval_name, cval_value, "Description.");
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
  base::CVal<int32_t> cval3(names[3], 0, "Test.");
  base::CVal<int32_t> cval1(names[1], 0, "Test.");
  base::CVal<int32_t, base::CValPublic> cval0(names[0], 0, "Test.");
  base::CVal<int32_t, base::CValPublic> cval2(names[2], 0, "Test.");

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
                        const base::CValGenericValue& value) override {
      EXPECT_EQ(name, "S32");
      EXPECT_EQ(value.GetTypeId(), GetTypeId<int32_t>());
      EXPECT_EQ(value.IsNativeType<int32_t>(), true);
      EXPECT_EQ(value.AsNativeType<int32_t>(), 1);
      EXPECT_EQ(value.AsString(), "1");
      got_callback_ = true;
    }
    bool got_callback_;
  };

  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  base::CVal<int32_t> cval("S32", 0, "Test.");
  TestOnChangedHook on_changed_hook;
  EXPECT_EQ(on_changed_hook.got_callback_, false);
  cval = 1;
  EXPECT_EQ(on_changed_hook.got_callback_, true);
}

TEST(CValTest, HookReceiver) {
  base::CVal<int32_t> cval_a("cval_a", 10, "");

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

    base::CVal<std::string> cval_b("cval_b", "foo", "");
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

  base::CVal<int32_t> cv_int32_t("cv_int32_t", 10, "");
  cv_int32_t = 15;

  EXPECT_EQ(test_hook.value_changed_count_, 1);
}

namespace {
// Helper class for the RemoveAndRead test defined below.
class ReadCValThread : public base::SimpleThread {
 public:
  ReadCValThread(const char* test_cval_name, int num_read_cycles,
                 std::pair<std::string, std::string> valid_values,
                 base::WaitableEvent* thread_ready_event,
                 base::WaitableEvent* start_processing_event)
      : base::SimpleThread("ReadCValThread"),
        test_cval_name_(test_cval_name),
        num_read_cycles_(num_read_cycles),
        valid_values_(valid_values),
        thread_ready_event_(thread_ready_event),
        start_processing_event_(start_processing_event) {}

  void Run() override {
    thread_ready_event_->Signal();
    start_processing_event_->Wait();

    base::CValManager* cvm = base::CValManager::GetInstance();
    for (int i = 0; i < num_read_cycles_; ++i) {
      base::optional<std::string> result =
          cvm->GetValueAsString(test_cval_name_);
      if (result) {
        EXPECT_TRUE(*result == valid_values_.first ||
                    *result == valid_values_.second);
      }
    }
  }

 private:
  const char* test_cval_name_;
  int num_read_cycles_;
  std::pair<std::string, std::string> valid_values_;
  base::WaitableEvent* thread_ready_event_;
  base::WaitableEvent* start_processing_event_;
};

// Helper class for the RemoveAndRead test defined below.
class CreateDestroyCValThread : public base::SimpleThread {
 public:
  CreateDestroyCValThread(const char* test_cval_name,
                          int num_create_destroy_cycles,
                          std::pair<std::string, std::string> valid_values,
                          base::WaitableEvent* thread_ready_event,
                          base::WaitableEvent* start_processing_event)
      : base::SimpleThread("CreateDestroyCValThread"),
        test_cval_name_(test_cval_name),
        num_create_destroy_cycles_(num_create_destroy_cycles),
        valid_values_(valid_values),
        thread_ready_event_(thread_ready_event),
        start_processing_event_(start_processing_event) {}

  void Run() override {
    thread_ready_event_->Signal();
    start_processing_event_->Wait();

    for (int i = 0; i < num_create_destroy_cycles_; ++i) {
      base::CVal<std::string> test_cval(test_cval_name_, valid_values_.first,
                                        "Description");
      test_cval = valid_values_.second;
    }
  }

 private:
  const char* test_cval_name_;
  int num_create_destroy_cycles_;
  std::pair<std::string, std::string> valid_values_;
  base::WaitableEvent* thread_ready_event_;
  base::WaitableEvent* start_processing_event_;
};

}  // namespace

// Tests that we can create and destroy cvals no problem while simultaneously
// reading from them.  The test creates two threads, a reader thread and a
// creater/destroyer (and writer) thread.  These both attempt to access the same
// cval as fast as possible.
TEST(CValTest, RemoveAndRead) {
  const char* kTestCValName = "TestCVal";
  const int kNumReadCycles = 10000;
  const int kNuMCreateDestroyCycles = 10000;
  const std::pair<std::string, std::string> valid_values("hello", "66");
  base::WaitableEvent read_cval_thread_ready(true, false);
  base::WaitableEvent create_destroy_cval_thread_ready(true, false);
  base::WaitableEvent start_processing_event(true, false);

  // Create and start both threads.
  ReadCValThread read_cval_thread(kTestCValName, kNumReadCycles, valid_values,
                                  &read_cval_thread_ready,
                                  &start_processing_event);
  CreateDestroyCValThread create_destroy_cval_thread(
      kTestCValName, kNuMCreateDestroyCycles, valid_values,
      &create_destroy_cval_thread_ready, &start_processing_event);
  read_cval_thread.Start();
  create_destroy_cval_thread.Start();

  // Wait until both threads are initialized and ready.
  read_cval_thread_ready.Wait();
  create_destroy_cval_thread_ready.Wait();

  // Signal for the processing/testing to begin.
  start_processing_event.Signal();

  // Wait for both threads to complete.
  read_cval_thread.Join();
  create_destroy_cval_thread.Join();
}

}  // namespace base
