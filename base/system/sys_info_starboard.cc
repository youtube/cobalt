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

#include "base/system/sys_info_starboard.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <sys/system_properties.h>
#endif  // BUILDFLAG(IS_ANDROID)

namespace base {
namespace starboard {

#if BUILDFLAG(IS_ANDROID)
std::string SbSysInfo::OriginalDesignManufacturer() {
  char original_design_manufacturer_str[PROP_VALUE_MAX];
  __system_property_get("ro.product.manufacturer", original_design_manufacturer_str);
  return std::string(original_design_manufacturer_str);
}

std::string SbSysInfo::ChipsetModelNumber() {
  char chipset_model_number_str[PROP_VALUE_MAX];
  __system_property_get("ro.board.platform", chipset_model_number_str);
  return std::string(chipset_model_number_str);
}

std::string SbSysInfo::ModelYear() {
  const std::string kUnknownValue = "unknown";

  char model_year_cstr[PROP_VALUE_MAX];
  __system_property_get("ro.oem.key1", model_year_cstr);
  std::string model_year_str(model_year_cstr); 
  
  if (model_year_str == kUnknownValue || model_year_str.length() < 10) {
    return model_year_str; 
  } 
  
  // See
  // https://support.google.com/androidpartners_androidtv/answer/9351639?hl=en
  // for the format of |model_year_str|.
  std::string year_str = "20";
  year_str += model_year_cstr[9];
  year_str += model_year_cstr[10];
  return year_str;
}

std::string SbSysInfo::Brand() {
  char brand_str[PROP_VALUE_MAX];
  __system_property_get("ro.product.brand", brand_str);
  return std::string(brand_str);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace starboard
}  // namespace base