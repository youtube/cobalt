// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/ppd_resolution_state.h"

#include "chromeos/printing/printer_configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class PpdResolutionStateTest : public testing::Test {
 public:
  PpdResolutionStateTest() = default;

  PpdResolutionStateTest(const PpdResolutionStateTest&) = delete;
  PpdResolutionStateTest& operator=(const PpdResolutionStateTest&) = delete;

  ~PpdResolutionStateTest() override = default;
};

TEST_F(PpdResolutionStateTest, TestDefaultState) {
  PpdResolutionState ppd_resolution_state_;

  EXPECT_TRUE(ppd_resolution_state_.IsInflight());
  EXPECT_FALSE(ppd_resolution_state_.WasResolutionSuccessful());
}

TEST_F(PpdResolutionStateTest, TestMarkPpdResolutionSucessful) {
  PpdResolutionState ppd_resolution_state_;

  const std::string expected_make_and_model = "printer_make_model";
  chromeos::Printer::PpdReference ppd_ref;
  ppd_ref.effective_make_and_model = expected_make_and_model;

  ppd_resolution_state_.MarkResolutionSuccessful(ppd_ref);

  EXPECT_FALSE(ppd_resolution_state_.IsInflight());
  EXPECT_TRUE(ppd_resolution_state_.WasResolutionSuccessful());

  const chromeos::Printer::PpdReference ref =
      ppd_resolution_state_.GetPpdReference();

  EXPECT_EQ(expected_make_and_model, ref.effective_make_and_model);
  EXPECT_TRUE(ref.user_supplied_ppd_url.empty());
  EXPECT_FALSE(ref.autoconf);
}

TEST_F(PpdResolutionStateTest, TestMarkResolutionFailedAndSetUsbManufacturer) {
  PpdResolutionState ppd_resolution_state_;

  ppd_resolution_state_.MarkResolutionFailed();

  EXPECT_FALSE(ppd_resolution_state_.IsInflight());
  EXPECT_FALSE(ppd_resolution_state_.WasResolutionSuccessful());

  const std::string expected_usb_manufacturer = "Hewlett-Packard";
  ppd_resolution_state_.SetUsbManufacturer(expected_usb_manufacturer);

  EXPECT_EQ(expected_usb_manufacturer,
            ppd_resolution_state_.GetUsbManufacturer());
}

}  // namespace
}  // namespace ash
