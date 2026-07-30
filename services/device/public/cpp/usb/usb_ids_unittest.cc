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

TEST(UsbIdsTest, GetVendorAndProductName) {
  // Unknown vendor: both names are nullptr.
  UsbIdNames unknown_vendor = UsbIds::GetVendorAndProductName(0, 0);
  EXPECT_EQ(nullptr, unknown_vendor.vendor_name);
  EXPECT_EQ(nullptr, unknown_vendor.product_name);

  // Known vendor, unknown product: vendor name is set, product name is not.
  UsbIdNames unknown_product =
      UsbIds::GetVendorAndProductName(kGoogleVendorId, 0);
  EXPECT_EQ(std::string("Google Inc."), unknown_product.vendor_name);
  EXPECT_EQ(nullptr, unknown_product.product_name);

  // Known vendor and product: both names are set.
  UsbIdNames known =
      UsbIds::GetVendorAndProductName(kGoogleVendorId, kNexusSProductId);
  EXPECT_EQ(std::string("Google Inc."), known.vendor_name);
  EXPECT_EQ(std::string("Nexus S"), known.product_name);
}

TEST(UsbIdsTest, UnknownVendorAndProduct) {
  // 0xffff is unallocated in usb.ids.
  EXPECT_EQ(nullptr, UsbIds::GetVendorName(0xffff));
  UsbIdNames unknown_vendor = UsbIds::GetVendorAndProductName(0xffff, 0x0001);
  EXPECT_EQ(nullptr, unknown_vendor.vendor_name);
  EXPECT_EQ(nullptr, unknown_vendor.product_name);
  // Known vendor, unknown product id far above any defined Google product.
  UsbIdNames unknown_product =
      UsbIds::GetVendorAndProductName(kGoogleVendorId, 0xffff);
  EXPECT_EQ(std::string("Google Inc."), unknown_product.vendor_name);
  EXPECT_EQ(nullptr, unknown_product.product_name);
}

void NameLookupsReturnValidStrings(uint16_t vendor_id, uint16_t product_id) {
  // Copying into a std::string forces a full read so sanitizers catch a bad
  // pointer or missing terminator.
  const char* vendor_name = UsbIds::GetVendorName(vendor_id);
  [[maybe_unused]] std::string vendor_string = vendor_name ? vendor_name : "";

  UsbIdNames names = UsbIds::GetVendorAndProductName(vendor_id, product_id);
  [[maybe_unused]] std::string combined_vendor_string =
      names.vendor_name ? names.vendor_name : "";
  [[maybe_unused]] std::string product_string =
      names.product_name ? names.product_name : "";
}
FUZZ_TEST(UsbIdsTest, NameLookupsReturnValidStrings);

}  // namespace device
