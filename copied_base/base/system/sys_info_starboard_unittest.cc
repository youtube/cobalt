// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <string>

#include "base/system/sys_info_starboard.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base {
namespace starboard {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS_TVOS)

using SbSysInfoTest = PlatformTest;

TEST_F(SbSysInfoTest, OriginalDesignManufacturer) {
  std::string original_design_manufacturer_str = SbSysInfo::OriginalDesignManufacturer();
  EXPECT_NE(original_design_manufacturer_str, "");
}

TEST_F(SbSysInfoTest, ChipsetModelNumber) {
  std::string chipset_model_number_str = SbSysInfo::ChipsetModelNumber();
  EXPECT_NE(chipset_model_number_str, "");
}

TEST_F(SbSysInfoTest, ModelYear) {
  std::string model_year_str = SbSysInfo::ModelYear();
  EXPECT_NE(model_year_str, "");
}

TEST_F(SbSysInfoTest, Brand) {
  std::string brand_str = SbSysInfo::Brand();
  EXPECT_NE(brand_str, "");
}

#endif

}  // namespace starboard
}  // namespace base
