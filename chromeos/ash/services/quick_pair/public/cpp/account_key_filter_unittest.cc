// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/public/cpp/account_key_filter.h"

#include <cstdint>
#include <iterator>

#include "base/no_destructor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

constexpr uint8_t kSalt = 0xC7;

const std::vector<uint8_t>& GetAccountKey1() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
       0xCC, 0xDD, 0xEE, 0xFF});
  return *val;
}

const std::vector<uint8_t>& GetFilterBytes1() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x0A, 0x42, 0x88, 0x10});
  return *val;
}

const std::vector<uint8_t>& GetAccountKey2() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66,
       0x77, 0x77, 0x88, 0x88});
  return *val;
}

const std::vector<uint8_t>& GetFilterBytes1And2() {
  static const base::NoDestructor<std::vector<uint8_t>> val(
      {0x2F, 0xBA, 0x06, 0x42, 0x00});
  return *val;
}

class AccountKeyFilterTest : public testing::Test {};

TEST_F(AccountKeyFilterTest, EmptyFilter) {
  AccountKeyFilter filter({}, {});
  EXPECT_FALSE(filter.IsAccountKeyInFilter(GetAccountKey1()));
  EXPECT_FALSE(filter.IsAccountKeyInFilter(GetAccountKey2()));
}

TEST_F(AccountKeyFilterTest, EmptyVectorTest) {
  EXPECT_FALSE(AccountKeyFilter(GetFilterBytes1(), {kSalt})
                   .IsAccountKeyInFilter(std::vector<uint8_t>(0)));
}

TEST_F(AccountKeyFilterTest, SingleAccountKey_AsBytes) {
  EXPECT_TRUE(AccountKeyFilter(GetFilterBytes1(), {kSalt})
                  .IsAccountKeyInFilter(GetAccountKey1()));
}

TEST_F(AccountKeyFilterTest, TwoAccountKeys_AsBytes) {
  AccountKeyFilter filter(GetFilterBytes1And2(), {kSalt});

  EXPECT_TRUE(filter.IsAccountKeyInFilter(GetAccountKey1()));
  EXPECT_TRUE(filter.IsAccountKeyInFilter(GetAccountKey2()));
}

TEST_F(AccountKeyFilterTest, MissingAccountKey) {
  const std::vector<uint8_t> account_key{0x12, 0x22, 0x33, 0x44, 0x55, 0x66,
                                         0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                         0xCC, 0xDD, 0xEE, 0xFF};

  EXPECT_FALSE(AccountKeyFilter(GetFilterBytes1(), {kSalt})
                   .IsAccountKeyInFilter(account_key));
  EXPECT_FALSE(AccountKeyFilter(GetFilterBytes1And2(), {kSalt})
                   .IsAccountKeyInFilter(account_key));
}

TEST_F(AccountKeyFilterTest, WithBatteryData) {
  const std::vector<uint8_t> filter_with_battery{0x4A, 0x00, 0xF0, 0x00};
  const std::vector<uint8_t> battery_data{0b00110011, 0b01000000, 0b01000000,
                                          0b01000000};
  std::vector<uint8_t> salt_values = {kSalt};
  for (auto& byte : battery_data) {
    salt_values.push_back(byte);
  }

  EXPECT_TRUE(AccountKeyFilter(filter_with_battery, salt_values)
                  .IsAccountKeyInFilter(GetAccountKey1()));
}

TEST_F(AccountKeyFilterTest, TwoSaltBytes) {
  const std::vector<uint8_t> account_key3{0x04, 0x70, 0xBA, 0xF8, 0x73, 0x19,
                                          0xCE, 0xF3, 0xA7, 0x97, 0x78, 0x52,
                                          0xB3, 0x4F, 0x4C, 0xD8};
  const std::vector<uint8_t> filter3{0xB8, 0x2A, 0x41, 0xE2, 0x21};
  const std::vector<uint8_t> salt3{0x6C, 0xE5};
  const std::vector<uint8_t> battery_data3{0x33, 0xE4, 0xE4, 0x4C};

  // Devices with battery data create account filters by concatenating data to
  // end of salt bytes
  std::vector<uint8_t> salt_values{};
  salt_values.insert(salt_values.end(), salt3.begin(), salt3.end());
  salt_values.insert(salt_values.end(), battery_data3.begin(),
                     battery_data3.end());

  EXPECT_TRUE(AccountKeyFilter(filter3, salt_values)
                  .IsAccountKeyInFilter(account_key3));
}

// Regression test for b/243855406.
TEST_F(AccountKeyFilterTest, SassEnabledPeripheral) {
  // Values for SASS enabled Pixel Buds Pro that failed to subsequent pair prior
  // to ccrev.com/3953062 landing.
  // Value source: b/243855406#comment24.
  const std::vector<uint8_t> account_key4{0x04, 0x3F, 0xC1, 0x8C, 0x63, 0xDC,
                                          0x75, 0x1A, 0xE8, 0x1A, 0xCF, 0x65,
                                          0x10, 0x15, 0x1D, 0xB0};
  const std::vector<uint8_t> filter4{0x19, 0x23, 0x50, 0xE8, 0x37,
                                     0x68, 0xF0, 0x65, 0x22};
  const std::vector<uint8_t> salt4{0xD7, 0xDE};
  const std::vector<uint8_t> battery_data4{0x33, 0xE4, 0xE4, 0x64};

  // Account Key, Salt, and Battery Data values used are specific to an observed
  // SASS device failure explained at the variable declaration above.
  std::vector<uint8_t> salt_values{};
  salt_values.insert(salt_values.end(), salt4.begin(), salt4.end());
  salt_values.insert(salt_values.end(), battery_data4.begin(),
                     battery_data4.end());
  EXPECT_TRUE(AccountKeyFilter(filter4, salt_values)
                  .IsAccountKeyInFilter(account_key4));
}

}  // namespace quick_pair
}  // namespace ash
