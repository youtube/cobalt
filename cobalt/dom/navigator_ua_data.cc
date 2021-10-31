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

#include "cobalt/dom/navigator_ua_data.h"

#include "base/strings/stringprintf.h"

namespace cobalt {
namespace dom {

NavigatorUAData::NavigatorUAData(
    UserAgentPlatformInfo* platform_info,
    script::ScriptValueFactory* script_value_factory)
    : script_value_factory_(script_value_factory) {
  if (platform_info == nullptr) {
    SB_DLOG(WARNING)
        << "No UserAgentPlatformInfo object passed to NavigatorUAData";
    return;
  }
  NavigatorUABrandVersion cobalt_brand;
  cobalt_brand.set_brand("Cobalt");
  cobalt_brand.set_version(platform_info->cobalt_version());
  brands_.push_back(cobalt_brand);

  mobile_ = false;
  platform_ = platform_info->os_name_and_version();

  all_high_entropy_values_.set_brands(brands_);
  all_high_entropy_values_.set_mobile(mobile_);
  all_high_entropy_values_.set_platform(platform_);
#if SB_API_VERSION >= 12
  all_high_entropy_values_.set_architecture(SB_SABI_TARGET_ARCH);
  all_high_entropy_values_.set_bitness(SB_SABI_WORD_SIZE);
#endif
  all_high_entropy_values_.set_model(platform_info->model().value_or(""));
  all_high_entropy_values_.set_platform_version(
      platform_info->firmware_version().value_or(""));
  all_high_entropy_values_.set_ua_full_version(
      base::StringPrintf("%s.%s-%s", platform_info->cobalt_version().c_str(),
                         platform_info->cobalt_build_version_number().c_str(),
                         platform_info->build_configuration().c_str()));
  all_high_entropy_values_.set_cobalt_build_number(
      platform_info->cobalt_build_version_number());
  all_high_entropy_values_.set_cobalt_build_configuration(
      platform_info->build_configuration());
  all_high_entropy_values_.set_js_engine_version(
      platform_info->javascript_engine_version());
  all_high_entropy_values_.set_rasterizer(platform_info->rasterizer_type());
  all_high_entropy_values_.set_evergreen_type(platform_info->evergreen_type());
  all_high_entropy_values_.set_evergreen_version(
      platform_info->evergreen_version());
  all_high_entropy_values_.set_starboard_version(
      platform_info->starboard_version());
  all_high_entropy_values_.set_original_design_manufacturer(
      platform_info->original_design_manufacturer().value_or(""));
  all_high_entropy_values_.set_device_type(platform_info->device_type_string());
  all_high_entropy_values_.set_chipset(
      platform_info->chipset_model_number().value_or(""));
  all_high_entropy_values_.set_model_year(
      platform_info->model_year().value_or(""));
  all_high_entropy_values_.set_device_brand(
      platform_info->brand().value_or(""));
  all_high_entropy_values_.set_connection_type(
      platform_info->connection_type_string());
  all_high_entropy_values_.set_aux(platform_info->aux_field());

  low_entropy_json_.set_brands(brands_);
  low_entropy_json_.set_mobile(mobile_);
  low_entropy_json_.set_platform(platform_);
}

script::Handle<NavigatorUAData::InterfacePromise>
NavigatorUAData::GetHighEntropyValues(script::Sequence<std::string> hints) {
  // https://wicg.github.io/ua-client-hints/#getHighEntropyValues
  CobaltUADataValues select_high_entropy_values_;

  // Set brands, mobile, and platform
  select_high_entropy_values_.set_brands(all_high_entropy_values_.brands());
  select_high_entropy_values_.set_mobile(all_high_entropy_values_.mobile());
  select_high_entropy_values_.set_platform(all_high_entropy_values_.platform());

  // Set other hints if specified
  for (script::Sequence<std::string>::iterator it = hints.begin();
       it != hints.end(); ++it) {
    if ((*it).compare("architecture") == 0) {
      select_high_entropy_values_.set_architecture(
          all_high_entropy_values_.architecture());
    } else if ((*it).compare("bitness") == 0) {
      select_high_entropy_values_.set_bitness(
          all_high_entropy_values_.bitness());
    } else if ((*it).compare("model") == 0) {
      select_high_entropy_values_.set_model(all_high_entropy_values_.model());
    } else if ((*it).compare("platformVersion") == 0) {
      select_high_entropy_values_.set_platform_version(
          all_high_entropy_values_.platform_version());
    } else if ((*it).compare("uaFullVersion") == 0) {
      select_high_entropy_values_.set_ua_full_version(
          all_high_entropy_values_.ua_full_version());
    } else if ((*it).compare("cobaltBuildNumber") == 0) {
      select_high_entropy_values_.set_cobalt_build_number(
          all_high_entropy_values_.cobalt_build_number());
    } else if ((*it).compare("cobaltBuildConfiguration") == 0) {
      select_high_entropy_values_.set_cobalt_build_configuration(
          all_high_entropy_values_.cobalt_build_configuration());
    } else if ((*it).compare("jsEngineVersion") == 0) {
      select_high_entropy_values_.set_js_engine_version(
          all_high_entropy_values_.js_engine_version());
    } else if ((*it).compare("rasterizer") == 0) {
      select_high_entropy_values_.set_rasterizer(
          all_high_entropy_values_.rasterizer());
    } else if ((*it).compare("evergreenType") == 0) {
      select_high_entropy_values_.set_evergreen_type(
          all_high_entropy_values_.evergreen_type());
    } else if ((*it).compare("evergreenVersion") == 0) {
      select_high_entropy_values_.set_evergreen_version(
          all_high_entropy_values_.evergreen_version());
    } else if ((*it).compare("starboardVersion") == 0) {
      select_high_entropy_values_.set_starboard_version(
          all_high_entropy_values_.starboard_version());
    } else if ((*it).compare("originalDesignManufacturer") == 0) {
      select_high_entropy_values_.set_original_design_manufacturer(
          all_high_entropy_values_.original_design_manufacturer());
    } else if ((*it).compare("deviceType") == 0) {
      select_high_entropy_values_.set_device_type(
          all_high_entropy_values_.device_type());
    } else if ((*it).compare("chipset") == 0) {
      select_high_entropy_values_.set_chipset(
          all_high_entropy_values_.chipset());
    } else if ((*it).compare("modelYear") == 0) {
      select_high_entropy_values_.set_model_year(
          all_high_entropy_values_.model_year());
    } else if ((*it).compare("deviceBrand") == 0) {
      select_high_entropy_values_.set_device_brand(
          all_high_entropy_values_.device_brand());
    } else if ((*it).compare("connectionType") == 0) {
      select_high_entropy_values_.set_connection_type(
          all_high_entropy_values_.connection_type());
    } else if ((*it).compare("aux") == 0) {
      select_high_entropy_values_.set_aux(all_high_entropy_values_.aux());
    }
  }

  script::Handle<InterfacePromise> promise =
      script_value_factory_->CreateInterfacePromise<
          scoped_refptr<CobaltUADataValuesInterface>>();
  scoped_refptr<CobaltUADataValuesInterface> promise_result(
      new CobaltUADataValuesInterface(select_high_entropy_values_));
  promise->Resolve(promise_result);
  return promise;
}

}  // namespace dom
}  // namespace cobalt
