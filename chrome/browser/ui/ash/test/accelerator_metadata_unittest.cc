// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>

#include "ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h"
#include "base/hash/md5.h"
#include "base/hash/md5_boringssl.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Internal builds add two extra accelerator for the Feedback app.
// The total number of Chrome accelerators (available on Chrome OS).
constexpr int kChromeAcceleratorsTotalNum = 103;
// The hash of Chrome accelerators (available on Chrome OS).
constexpr char kChromeAcceleratorsHash[] = "0b83abd23bca45738c58668d94337f48";
#else
// The total number of Chrome accelerators (available on Chrome OS).
constexpr int kChromeAcceleratorsTotalNum = 101;
// The hash of Chrome accelerators (available on Chrome OS).
constexpr char kChromeAcceleratorsHash[] = "b294aad5a7b11a754d8c81940979e47c";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

const char kCommonMessage[] =
    "If you are modifying Chrome OS available shortcuts, please update "
    "kAcceleratorLayouts & following the instruction in "
    "ash/webui/shortcut_customization_ui/backend/"
    "accelerator_layout_table.h and the following value(s) on the "
    "top of this file:\n";

std::string ModifiersToString(int modifiers) {
  return base::StringPrintf(
      "shift=%s control=%s alt=%s search=%s",
      base::ToString<bool>(modifiers & ui::EF_SHIFT_DOWN),
      base::ToString<bool>(modifiers & ui::EF_CONTROL_DOWN),
      base::ToString<bool>(modifiers & ui::EF_ALT_DOWN),
      base::ToString<bool>(modifiers & ui::EF_COMMAND_DOWN));
}

struct ChromeAcceleratorMappingCmp {
  bool operator()(const AcceleratorMapping& lhs,
                  const AcceleratorMapping& rhs) {
    return std::tie(lhs.keycode, lhs.modifiers) <
           std::tie(rhs.keycode, rhs.modifiers);
  }
};

std::string ChromeAcceleratorMappingToString(
    const AcceleratorMapping& accelerator) {
  return base::StringPrintf("keycode=%d command_id=%d ", accelerator.keycode,
                            accelerator.command_id) +
         ModifiersToString(accelerator.modifiers);
}

std::string HashChromeAcceleratorMapping(
    const std::vector<AcceleratorMapping>& accelerators) {
  base::MD5Context context;
  base::MD5Init(&context);
  for (const auto& accelerator : accelerators) {
    base::MD5Update(&context, ChromeAcceleratorMappingToString(accelerator));
  }

  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

class ChromeAcceleratorMetadataTest : public testing::Test {
 public:
  ChromeAcceleratorMetadataTest() = default;

  ChromeAcceleratorMetadataTest(const ChromeAcceleratorMetadataTest&) = delete;
  ChromeAcceleratorMetadataTest& operator=(
      const ChromeAcceleratorMetadataTest&) = delete;

  ~ChromeAcceleratorMetadataTest() override = default;
};

}  // namespace

// Test that modifying chrome accelerator should update kAcceleratorLayouts.
// 1. If you are adding/deleting/modifying shortcuts, please also
//    add/delete/modify the corresponding item in kAcceleratorLayouts.
// 2. Please update the number and hash value of chrome accelerators on the top
// of this file. The new number and hash value will be provided in the test
// output.
TEST_F(ChromeAcceleratorMetadataTest,
       ModifyChromeAcceleratorShouldUpdateLayout) {
  std::vector<AcceleratorMapping> chrome_accelerators;
  for (const auto& accel_mapping : GetAcceleratorList()) {
    chrome_accelerators.emplace_back(accel_mapping);
  }

  const int chrome_accelerators_number = chrome_accelerators.size();
  EXPECT_EQ(chrome_accelerators_number, kChromeAcceleratorsTotalNum)
      << kCommonMessage
      << "kChromeAcceleratorsTotalNum=" << chrome_accelerators_number << "\n";

  std::stable_sort(chrome_accelerators.begin(), chrome_accelerators.end(),
                   ChromeAcceleratorMappingCmp());
  const std::string chrome_accelerators_hash =
      HashChromeAcceleratorMapping(chrome_accelerators);
  EXPECT_EQ(chrome_accelerators_hash, kChromeAcceleratorsHash)
      << kCommonMessage << "kChromeAcceleratorsHash=\""
      << chrome_accelerators_hash << "\"\n";
}

}  // namespace ash
