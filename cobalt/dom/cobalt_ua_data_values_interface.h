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
  explicit CobaltUADataValuesInterface(const CobaltUADataValues& init_dict) {
    // In many cases, |init_dict| will not have all its fields initialized, only
    // the ones specified when calling NavigatorUAData::getHighEntropyValues().
    // Thus, we first must check whether the fields have been initialized.
    if (init_dict.has_cobalt_build_number()) {
      cobalt_build_number_ = init_dict.cobalt_build_number();
    }
    if (init_dict.has_cobalt_build_configuration()) {
      cobalt_build_configuration_ = init_dict.cobalt_build_configuration();
    }
    if (init_dict.has_js_engine_version()) {
      js_engine_version_ = init_dict.js_engine_version();
    }
    if (init_dict.has_rasterizer()) {
      rasterizer_ = init_dict.rasterizer();
    }
    if (init_dict.has_evergreen_version()) {
      evergreen_version_ = init_dict.evergreen_version();
    }
    if (init_dict.has_starboard_version()) {
      starboard_version_ = init_dict.starboard_version();
    }
    if (init_dict.has_original_design_manufacturer()) {
      original_design_manufacturer_ = init_dict.original_design_manufacturer();
    }
    if (init_dict.has_device_type()) {
      device_type_ = init_dict.device_type();
    }
    if (init_dict.has_chipset()) {
      chipset_ = init_dict.chipset();
    }
    if (init_dict.has_model_year()) {
      model_year_ = init_dict.model_year();
    }
    if (init_dict.has_device_brand()) {
      device_brand_ = init_dict.device_brand();
    }
    if (init_dict.has_connection_type()) {
      connection_type_ = init_dict.connection_type();
    }
    if (init_dict.has_aux()) {
      aux_ = init_dict.aux();
    }
  }

  const std::string& cobalt_build_number() const {
    return cobalt_build_number_;
  }
  const std::string& cobalt_build_configuration() const {
    return cobalt_build_configuration_;
  }
  const std::string& js_engine_version() const { return js_engine_version_; }
  const std::string& rasterizer() const { return rasterizer_; }
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

  std::string cobalt_build_number_;
  std::string cobalt_build_configuration_;
  std::string js_engine_version_;
  std::string rasterizer_;
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
