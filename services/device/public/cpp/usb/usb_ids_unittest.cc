// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/usb/usb_ids.h"

#include <stdint.h>

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace {

constexpr uint16_t kGoogleVendorId = 0x18d1;
constexpr uint16_t kNexusSProductId = 0x4e21;

}  // namespace

namespace device {

TEST(UsbIdsTest, GetVendorName) {
  EXPECT_EQ(nullptr, UsbIds::GetVendorName(0));
  // Implicitly verifies that IDR_USB_IDS_DATA is shipped and parseable: a
  // missing or malformed resource would return nullptr here.
  EXPECT_EQ(std::string("Google Inc."), UsbIds::GetVendorName(kGoogleVendorId));
}

TEST(UsbIdsTest, GetProductName) {
  EXPECT_EQ(nullptr, UsbIds::GetProductName(0, 0));
  EXPECT_EQ(nullptr, UsbIds::GetProductName(kGoogleVendorId, 0));
  EXPECT_EQ(std::string("Nexus S"),
            UsbIds::GetProductName(kGoogleVendorId, kNexusSProductId));
}

TEST(UsbIdsTest, UnknownVendorAndProduct) {
  // 0xffff is unallocated in usb.ids.
  EXPECT_EQ(nullptr, UsbIds::GetVendorName(0xffff));
  EXPECT_EQ(nullptr, UsbIds::GetProductName(0xffff, 0x0001));
  // Known vendor, unknown product id far above any defined Google product.
  EXPECT_EQ(nullptr, UsbIds::GetProductName(kGoogleVendorId, 0xffff));
}

void NameLookupsReturnValidStrings(uint16_t vendor_id, uint16_t product_id) {
  // Copying into a std::string forces a full read so sanitizers catch a bad
  // pointer or missing terminator.
  const char* vendor_name = UsbIds::GetVendorName(vendor_id);
  [[maybe_unused]] std::string vendor_string = vendor_name ? vendor_name : "";

  const char* product_name = UsbIds::GetProductName(vendor_id, product_id);
  [[maybe_unused]] std::string product_string =
      product_name ? product_name : "";
}
FUZZ_TEST(UsbIdsTest, NameLookupsReturnValidStrings);

}  // namespace device
