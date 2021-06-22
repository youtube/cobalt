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

#include "cobalt/dom/cobalt_ua_data_values_interface.h"

namespace cobalt {
namespace dom {

CobaltUADataValuesInterface::CobaltUADataValuesInterface(
    const CobaltUADataValues& init_dict) {
  // In many cases, |init_dict| will not have all its fields initialized, only
  // the ones specified when calling NavigatorUAData::getHighEntropyValues().
  // Thus, we first must check whether the fields have been initialized.
  if (init_dict.has_brands()) {
    brands_ = init_dict.brands();
  }
  if (init_dict.has_mobile()) {
    mobile_ = init_dict.mobile();
  }
  if (init_dict.has_platform()) {
    platform_ = init_dict.platform();
  }
  if (init_dict.has_architecture()) {
    architecture_ = init_dict.architecture();
  }
  if (init_dict.has_bitness()) {
    bitness_ = init_dict.bitness();
  }
  if (init_dict.has_model()) {
    model_ = init_dict.model();
  }
  if (init_dict.has_platform()) {
    platform_ = init_dict.platform();
  }
  if (init_dict.has_ua_full_version()) {
    ua_full_version_ = init_dict.ua_full_version();
  }
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
  if (init_dict.has_evergreen_type()) {
    evergreen_type_ = init_dict.evergreen_type();
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

}  // namespace dom
}  // namespace cobalt
