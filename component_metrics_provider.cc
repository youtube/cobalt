// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/component_metrics_provider.h"

#include <map>
#include <string>
#include "base/strings/string_number_conversions.h"
#include "components/component_updater/component_updater_service.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

namespace {

SystemProfileProto_ComponentId CrxIdToComponentId(const std::string& app_id) {
  const static std::map<std::string, SystemProfileProto_ComponentId>
      component_map = {
          {"khaoiebndkojlmppeemjhbpbandiljpe",
           SystemProfileProto_ComponentId_FILE_TYPE_POLICIES},
          {"kfoklmclfodeliojeaekpoflbkkhojea",
           SystemProfileProto_ComponentId_ORIGIN_TRIALS},
          {"llkgjffcdpffmhiakmfcdcblohccpfmo",
           SystemProfileProto_ComponentId_ORIGIN_TRIALS},  // Alternate ID
          {"mimojjlkmoijpicakmndhoigimigcmbb",
           SystemProfileProto_ComponentId_PEPPER_FLASH},
          {"ckjlcfmdbdglblbjglepgnoekdnkoklc",
           SystemProfileProto_ComponentId_PEPPER_FLASH_CHROMEOS},
          {"hnimpnehoodheedghdeeijklkeaacbdc",
           SystemProfileProto_ComponentId_PNACL},
          {"npdjjkjlcidkjlamlmmdelcjbcpdjocm",
           SystemProfileProto_ComponentId_RECOVERY},
          {"giekcmmlnklenlaomppkphknjmnnpneh",
           SystemProfileProto_ComponentId_SSL_ERROR_ASSISTANT},
          {"ojjgnpkioondelmggbekfhllhdaimnho",
           SystemProfileProto_ComponentId_STH_SET},
          {"hfnkpimlhhgieaddgfemjhofmfblmnib",
           SystemProfileProto_ComponentId_CRL_SET},
          {"gcmjkmgdlgnkkcocmoeiminaijmmjnii",
           SystemProfileProto_ComponentId_SUBRESOURCE_FILTER},
          {"gkmgaooipdjhmangpemjhigmamcehddo",
           SystemProfileProto_ComponentId_SW_REPORTER},
          {"oimompecagnajdejgnnjijobebaeigek",
           SystemProfileProto_ComponentId_WIDEVINE_CDM},
          {"bjbdkfoakgmkndalgpadobhgbhhoanho",
           SystemProfileProto_ComponentId_EPSON_INKJET_PRINTER_ESCPR},
          {"ojnjgapiepgciobpecnafnoeaegllfld",
           SystemProfileProto_ComponentId_CROS_TERMINA},
          {"gncenodapghbnkfkoognegdnjoeegmkp",
           SystemProfileProto_ComponentId_STAR_CUPS_DRIVER},
          {"gelhpeofhffbaeegmemklllhfdifagmb",
           SystemProfileProto_ComponentId_SPEECH_SYNTHESIS_SV_SE},
          {"lmelglejhemejginpboagddgdfbepgmp",
           SystemProfileProto_ComponentId_OPTIMIZATION_HINTS},
          {"fookoiellkocclipolgaceabajejjcnp",
           SystemProfileProto_ComponentId_DOWNLOADABLE_STRINGS},
          {"cjfkbpdpjpdldhclahpfgnlhpodlpnba",
           SystemProfileProto_ComponentId_VR_ASSETS},
          {"gjpajnddmedjmcklfflllocelehklffm",
           SystemProfileProto_ComponentId_RTANALYTICS_LIGHT},
          {"mjdmdobabdmfcbaakcaadileafkmifen",
           SystemProfileProto_ComponentId_RTANALYTICS_FULL},
          {"fhbeibbmaepakgdkkmjgldjajgpkkhfj",
           SystemProfileProto_ComponentId_CELLULAR}};
  const auto result = component_map.find(app_id);
  if (result == component_map.end())
    return SystemProfileProto_ComponentId_UNKNOWN;
  return result->second;
}

// Extract the first 32 bits of a fingerprint string, excluding the fingerprint
// format specifier - see the fingerprint format specification at
// https://github.com/google/omaha/blob/master/doc/ServerProtocolV3.md
uint32_t Trim(const std::string& fp) {
  const auto len_prefix = fp.find(".");
  if (len_prefix == std::string::npos)
    return 0;
  uint32_t result = 0;
  if (base::HexStringToUInt(fp.substr(len_prefix + 1, 8), &result))
    return result;
  return 0;
}

}  // namespace

ComponentMetricsProvider::ComponentMetricsProvider(
    component_updater::ComponentUpdateService* component_update_service)
    : component_update_service_(component_update_service) {}

ComponentMetricsProvider::~ComponentMetricsProvider() = default;

void ComponentMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile) {
  for (const auto& component : component_update_service_->GetComponents()) {
    const auto id = CrxIdToComponentId(component.id);
    // Ignore any unknown components - in practice these are the
    // SupervisedUserWhitelists, which we do not want to transmit to UMA or
    // Crash.
    if (id == SystemProfileProto_ComponentId_UNKNOWN)
      continue;
    auto* proto = system_profile->add_chrome_component();
    proto->set_component_id(id);
    proto->set_version(component.version.GetString());
    proto->set_omaha_fingerprint(Trim(component.fingerprint));
  }
}

}  // namespace metrics
