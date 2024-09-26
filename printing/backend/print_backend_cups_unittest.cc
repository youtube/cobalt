// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_cups.h"

#include <cups/cups.h>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

bool IsDestTypeEligible(int dest_type) {
  cups_dest_t* dest = nullptr;
  int num_dests = 0;
  num_dests =
      cupsAddDest(/*name=*/"test_dest", /*instance=*/nullptr, num_dests, &dest);
  if (num_dests != 1)
    return false;

  cups_option_t* options = nullptr;
  int num_options = 0;
  num_options = cupsAddOption(kCUPSOptPrinterType,
                              base::NumberToString(dest_type).c_str(),
                              num_options, &options);
  dest->num_options = num_options;
  dest->options = options;

  PrinterBasicInfo printer_info;
  const mojom::ResultCode result_code =
      PrintBackendCUPS::PrinterBasicInfoFromCUPS(*dest, &printer_info);

  cupsFreeDests(num_dests, dest);
  return result_code == mojom::ResultCode::kSuccess;
}

}  // namespace

TEST(PrintBackendCupsTest, PrinterBasicInfoFromCUPS) {
  constexpr char kName[] = "printer";
  constexpr char kDescription[] = "description";
  cups_dest_t* printer = nullptr;
  ASSERT_EQ(
      1, cupsAddDest(kName, /*instance=*/nullptr, /*num_dests=*/0, &printer));

  int num_options = 0;
  cups_option_t* options = nullptr;
#if BUILDFLAG(IS_MAC)
  constexpr char kInfo[] = "info";
  num_options =
      cupsAddOption(kCUPSOptPrinterInfo, kInfo, num_options, &options);
  num_options = cupsAddOption(kCUPSOptPrinterMakeAndModel, kDescription,
                              num_options, &options);
  ASSERT_EQ(2, num_options);
  ASSERT_TRUE(options);
#else
  num_options =
      cupsAddOption(kCUPSOptPrinterInfo, kDescription, num_options, &options);
  ASSERT_EQ(1, num_options);
  ASSERT_TRUE(options);
#endif
  printer->num_options = num_options;
  printer->options = options;

  PrinterBasicInfo printer_info;
  EXPECT_EQ(PrintBackendCUPS::PrinterBasicInfoFromCUPS(*printer, &printer_info),
            mojom::ResultCode::kSuccess);
  cupsFreeDests(/*num_dests=*/1, printer);

  EXPECT_EQ(kName, printer_info.printer_name);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(kInfo, printer_info.display_name);
#else
  EXPECT_EQ(kName, printer_info.display_name);
#endif
  EXPECT_EQ(kDescription, printer_info.printer_description);

  // The option value of `kCUPSOptPrinterMakeAndModel` is used to set the value
  // for `kDriverInfoTagName`.
  auto driver = printer_info.options.find(kDriverInfoTagName);
#if BUILDFLAG(IS_MAC)
  ASSERT_NE(driver, printer_info.options.end());
  EXPECT_EQ(kDescription, driver->second);
#else
  // Didn't set option for `kCUPSOptPrinterMakeAndModel`.
  EXPECT_EQ(driver, printer_info.options.end());
#endif
}

TEST(PrintBackendCupsTest, PrinterDriverInfoFromCUPS) {
  constexpr char kName[] = "test-printer-name";
  constexpr char kDescription[] = "A test printer";
  cups_dest_t* printer = nullptr;
  ASSERT_EQ(
      1, cupsAddDest(kName, /*instance=*/nullptr, /*num_dests=*/0, &printer));

  int num_options = 0;
  cups_option_t* options = nullptr;
  num_options = cupsAddOption(kCUPSOptPrinterMakeAndModel, kDescription,
                              num_options, &options);
  ASSERT_EQ(1, num_options);
  ASSERT_TRUE(options);
  printer->num_options = num_options;
  printer->options = options;

  EXPECT_EQ(kDescription,
            PrintBackendCUPS::PrinterDriverInfoFromCUPS(*printer));

  cupsFreeDests(/*num_dests=*/1, printer);
}

TEST(PrintBackendCupsTest, EligibleDestTypes) {
  EXPECT_FALSE(IsDestTypeEligible(CUPS_PRINTER_FAX));
  EXPECT_FALSE(IsDestTypeEligible(CUPS_PRINTER_SCANNER));
  EXPECT_FALSE(IsDestTypeEligible(CUPS_PRINTER_DISCOVERED));
  EXPECT_TRUE(IsDestTypeEligible(CUPS_PRINTER_LOCAL));

  // Try combos. `CUPS_PRINTER_LOCAL` has a value of 0, but keep these test
  // cases in the event that the constant values change in CUPS.
  EXPECT_FALSE(IsDestTypeEligible(CUPS_PRINTER_LOCAL | CUPS_PRINTER_FAX));
  EXPECT_FALSE(IsDestTypeEligible(CUPS_PRINTER_LOCAL | CUPS_PRINTER_SCANNER));
}

}  // namespace printing
