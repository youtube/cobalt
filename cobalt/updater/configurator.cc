// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/configurator.h"

#include "base/version.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/updater/network_fetcher.h"
#include "cobalt/updater/patcher.h"
#include "cobalt/updater/prefs.h"
#include "cobalt/updater/unzipper.h"
#include "cobalt/updater/updater_constants.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/network.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzipper.h"

#include "url/gurl.h"

namespace {

// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

const std::string kSabiString =
    R"({"target_arch":"arm","target_arch_sub":"v7a","word_size":32,"endianness":"little","calling_convention":"eabi","floating_point_abi":"hard","floating_point_fpu":"vfpv3","signedness_of_char":"signed","alignment_char":1,"alignment_double":8,"alignment_float":4,"alignment_int":4,"alignment_llong":8,"alignment_long":4,"alignment_pointer":4,"alignment_short":2,"alignment_stack":4,"size_of_char":1,"size_of_double":8,"size_of_float":4,"size_of_int":4,"size_of_llong":8,"size_of_long":4,"size_of_pointer":4,"size_of_short":2})";
const std::string kUpdaterChannel = "dev";
}  // namespace

namespace cobalt {
namespace updater {

Configurator::Configurator(network::NetworkModule* network_module)
    : pref_service_(CreatePrefService()),
      unzip_factory_(base::MakeRefCounted<UnzipperFactory>()),
      network_fetcher_factory_(
          base::MakeRefCounted<NetworkFetcherFactoryCobalt>(network_module)),
      patch_factory_(base::MakeRefCounted<PatcherFactory>()) {}
Configurator::~Configurator() = default;

int Configurator::InitialDelay() const { return 0; }

int Configurator::NextCheckDelay() const { return 5 * kDelayOneHour; }

int Configurator::OnDemandDelay() const { return 0; }

int Configurator::UpdateDelay() const { return 0; }

std::vector<GURL> Configurator::UpdateUrl() const {
  return std::vector<GURL>{GURL(kUpdaterJSONDefaultUrl)};
}

std::vector<GURL> Configurator::PingUrl() const { return UpdateUrl(); }

std::string Configurator::GetProdId() const { return "cobalt"; }

base::Version Configurator::GetBrowserVersion() const {
  return base::Version("1.0.0.0");  // version_info::GetVersion();
}

std::string Configurator::GetChannel() const { return {}; }

std::string Configurator::GetBrand() const { return {}; }

std::string Configurator::GetLang() const { return {}; }

std::string Configurator::GetOSLongName() const {
  return "Starboard";  // version_info::GetOSType();
}

base::flat_map<std::string, std::string> Configurator::ExtraRequestParams()
    const {
  base::flat_map<std::string, std::string> params;
  // TODO: hook up the SABI string and updater channel.
  params.insert(std::make_pair("SABI", kSabiString));
  params.insert(std::make_pair("sbversion", std::to_string(SB_API_VERSION)));
  params.insert(std::make_pair(
      "jsengine", script::GetJavaScriptEngineNameAndVersion()));
  params.insert(std::make_pair("updaterchannel", kUpdaterChannel));
  return params;
}

std::string Configurator::GetDownloadPreference() const { return {}; }

scoped_refptr<update_client::NetworkFetcherFactory>
Configurator::GetNetworkFetcherFactory() {
  return network_fetcher_factory_;
}

scoped_refptr<update_client::UnzipperFactory>
Configurator::GetUnzipperFactory() {
  return unzip_factory_;
}

scoped_refptr<update_client::PatcherFactory> Configurator::GetPatcherFactory() {
  return patch_factory_;
}

bool Configurator::EnabledDeltas() const { return false; }

bool Configurator::EnabledComponentUpdates() const { return false; }

bool Configurator::EnabledBackgroundDownloader() const { return false; }

// TODO: enable cup signing
bool Configurator::EnabledCupSigning() const { return false; }

PrefService* Configurator::GetPrefService() const {
  return pref_service_.get();
}

update_client::ActivityDataService* Configurator::GetActivityDataService()
    const {
  return nullptr;
}

bool Configurator::IsPerUserInstall() const { return true; }

std::vector<uint8_t> Configurator::GetRunActionKeyHash() const { return {}; }

std::string Configurator::GetAppGuid() const {
  return "{6D4E53F3-CC64-4CB8-B6BD-AB0B8F300E1C}";
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
Configurator::GetProtocolHandlerFactory() const {
  return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
}

update_client::RecoveryCRXElevator Configurator::GetRecoveryCRXElevator()
    const {
  return {};
}

}  // namespace updater
}  // namespace cobalt
