// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_COBALT_UA_DATA_VALUES_INTERFACE_H_
#define COBALT_DOM_COBALT_UA_DATA_VALUES_INTERFACE_H_

#include <string>

#include "cobalt/dom/cobalt_ua_data_values.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class CobaltUADataValuesInterface : public script::Wrappable {
 public:
  explicit CobaltUADataValuesInterface(const CobaltUADataValues& init_dict);

  script::Sequence<NavigatorUABrandVersion> brands() const { return brands_; }
  bool mobile() const { return mobile_; }
  const std::string& platform() const { return platform_; }
  const std::string& architecture() const { return architecture_; }
  const std::string& bitness() const { return bitness_; }
  const std::string& model() const { return model_; }
  const std::string& platform_version() const { return platform_version_; }
  const std::string& ua_full_version() const { return ua_full_version_; }
  const std::string& cobalt_build_number() const {
    return cobalt_build_number_;
  }
  const std::string& cobalt_build_configuration() const {
    return cobalt_build_configuration_;
  }
  const std::string& js_engine_version() const { return js_engine_version_; }
  const std::string& rasterizer() const { return rasterizer_; }
  const std::string& evergreen_type() const { return evergreen_type_; }
  const std::string& evergreen_version() const { return evergreen_version_; }
  const std::string& starboard_version() const { return starboard_version_; }
  const std::string& original_design_manufacturer() const {
    return original_design_manufacturer_;
  }
  const std::string& device_type() const { return device_type_; }
  const std::string& chipset() const { return chipset_; }
  const std::string& model_year() const { return model_year_; }
  const std::string& device_brand() const { return device_brand_; }
  const std::string& connection_type() const { return connection_type_; }
  const std::string& aux() const { return aux_; }

  DEFINE_WRAPPABLE_TYPE(CobaltUADataValuesInterface);

 private:
  ~CobaltUADataValuesInterface() override {}

  script::Sequence<NavigatorUABrandVersion> brands_;
  bool mobile_;
  std::string platform_;
  std::string architecture_;
  std::string bitness_;
  std::string model_;
  std::string platform_version_;
  std::string ua_full_version_;
  std::string cobalt_build_number_;
  std::string cobalt_build_configuration_;
  std::string js_engine_version_;
  std::string rasterizer_;
  std::string evergreen_type_;
  std::string evergreen_version_;
  std::string starboard_version_;
  std::string original_design_manufacturer_;
  std::string device_type_;
  std::string chipset_;
  std::string model_year_;
  std::string device_brand_;
  std::string connection_type_;
  std::string aux_;

  DISALLOW_COPY_AND_ASSIGN(CobaltUADataValuesInterface);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_COBALT_UA_DATA_VALUES_INTERFACE_H_
