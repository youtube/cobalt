// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_blocklist.h"

#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

class SerialBlocklistTest : public testing::Test {
 public:
  void SetDynamicBlocklist(base::StringPiece value) {
    feature_list_.Reset();

    std::map<std::string, std::string> parameters;
    parameters[kWebSerialBlocklistAdditions.name] = std::string(value);
    feature_list_.InitWithFeaturesAndParameters(
        {{kWebSerialBlocklist, parameters}}, {});

    SerialBlocklist::Get().ResetToDefaultValuesForTesting();
  }

  device::mojom::SerialPortInfoPtr CreateInfo(uint16_t usb_vendor_id,
                                              uint16_t usb_product_id) {
    auto info = device::mojom::SerialPortInfo::New();
    info->has_vendor_id = true;
    info->vendor_id = usb_vendor_id;
    info->has_product_id = true;
    info->product_id = usb_product_id;
    return info;
  }

 private:
  void TearDown() override {
    // Because SerialBlocklist is a singleton it must be cleared after tests run
    // to prevent leakage between tests.
    feature_list_.Reset();
    SerialBlocklist::Get().ResetToDefaultValuesForTesting();
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SerialBlocklistTest, BasicExclusions) {
  SetDynamicBlocklist("usb:18D1:58F0");
  EXPECT_TRUE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D1, 0x58F0)));
  // Devices with nearby vendor and product IDs are not blocked.
  EXPECT_FALSE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D1, 0x58F1)));
  EXPECT_FALSE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D1, 0x58EF)));
  EXPECT_FALSE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D0, 0x58F0)));
  EXPECT_FALSE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D2, 0x58F0)));
}

TEST_F(SerialBlocklistTest, NonUsbDevice) {
  auto info = device::mojom::SerialPortInfo::New();
  info->has_vendor_id = false;
  info->has_product_id = false;
  EXPECT_FALSE(SerialBlocklist::Get().IsExcluded(*info));
}

TEST_F(SerialBlocklistTest, StringsWithNoValidEntries) {
  SetDynamicBlocklist("");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist("~!@#$%^&*()-_=+[]{}/*-");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist(":");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist("::");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist(",");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist(",,");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist(",::,");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist("usb:2:3");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist("usb:18D1:2");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist("usb:0000:0x00");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist("usb:0000:   0");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist("usb:000g:0000");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist("bluetooth:0000:0000");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());

  SetDynamicBlocklist("☯");
  EXPECT_EQ(0u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());
}

TEST_F(SerialBlocklistTest, StringsWithOneValidEntry) {
  SetDynamicBlocklist("usb:18D1:58F0");
  EXPECT_EQ(1u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());
  EXPECT_TRUE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D1, 0x58F0)));

  SetDynamicBlocklist(" usb:18D1:58F0  ");
  EXPECT_EQ(1u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());
  EXPECT_TRUE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D1, 0x58F0)));

  SetDynamicBlocklist(", usb:18D1:58F0,  ");
  EXPECT_EQ(1u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());
  EXPECT_TRUE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D1, 0x58F0)));

  SetDynamicBlocklist("usb:18D1:58F0, bluetooth:18D1:58F1");
  EXPECT_EQ(1u, SerialBlocklist::Get().GetDynamicEntryCountForTesting());
  EXPECT_TRUE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D1, 0x58F0)));
}

TEST_F(SerialBlocklistTest, StaticEntries) {
  EXPECT_TRUE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D1, 0x58F3)));
  // Devices with nearby vendor and product IDs are not blocked.
  EXPECT_FALSE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D1, 0x58F4)));
  EXPECT_FALSE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D1, 0x58F2)));
  EXPECT_FALSE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D0, 0x58F3)));
  EXPECT_FALSE(SerialBlocklist::Get().IsExcluded(*CreateInfo(0x18D2, 0x58F3)));
}
