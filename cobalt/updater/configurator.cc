// Copyright 2020 The Cobalt Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/configurator.h"

#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/version.h"
#include "cobalt/browser/switches.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/updater/network_fetcher.h"
#include "cobalt/updater/patcher.h"
#include "cobalt/updater/prefs.h"
#include "cobalt/updater/unzipper.h"
#include "cobalt/updater/updater_constants.h"
#include "cobalt/version.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/network.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzipper.h"
#include "starboard/common/system_property.h"
#include "starboard/system.h"
#include "url/gurl.h"

using starboard::kSystemPropertyMaxLength;

namespace {

// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;
const char kDefaultUpdaterChannel[] = "prod";
const char kOmahaCobaltTrunkAppID[] = "{A9557415-DDCD-4948-8113-C643EFCF710C}";
const char kOmahaCobaltAppID[] = "{6D4E53F3-CC64-4CB8-B6BD-AB0B8F300E1C}";

std::string GetDeviceProperty(SbSystemPropertyId id) {
  char value[kSystemPropertyMaxLength];
  bool result;
  result = SbSystemGetProperty(id, value, kSystemPropertyMaxLength);
  std::string prop;
  if (result) {
    prop = std::string(value);
  }
  return prop;
}

}  // namespace

namespace cobalt {
namespace updater {

Configurator::Configurator(network::NetworkModule* network_module)
    : pref_service_(CreatePrefService()),
      persisted_data_(std::make_unique<update_client::PersistedData>(
          pref_service_.get(), nullptr)),
      is_channel_changed_(0),
      unzip_factory_(base::MakeRefCounted<UnzipperFactory>()),
      network_fetcher_factory_(
          base::MakeRefCounted<NetworkFetcherFactoryCobalt>(network_module)),
      patch_factory_(base::MakeRefCounted<PatcherFactory>()) {
  LOG(INFO) << "Configurator::Configurator";
  const std::string persisted_channel =
      persisted_data_->GetUpdaterChannel(GetAppGuid());
  if (persisted_channel.empty()) {
    SetChannel(kDefaultUpdaterChannel);
  } else {
    SetChannel(persisted_channel);
  }
  if (network_module != nullptr) {
    user_agent_string_ = network_module->GetUserAgent();
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          browser::switches::kUseCompressedUpdates)) {
    use_compressed_updates_.store(true);
  } else {
    use_compressed_updates_.store(false);
  }
}
Configurator::~Configurator() { LOG(INFO) << "Configurator::~Configurator"; }

int Configurator::InitialDelay() const { return 0; }

int Configurator::NextCheckDelay() const { return 5 * kDelayOneHour; }

int Configurator::OnDemandDelay() const { return 0; }

int Configurator::UpdateDelay() const { return 0; }

std::vector<GURL> Configurator::UpdateUrl() const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          browser::switches::kUseQAUpdateServer)) {
    return std::vector<GURL>{GURL(kUpdaterJSONDefaultUrlQA)};
  } else {
    return std::vector<GURL>{GURL(kUpdaterJSONDefaultUrl)};
  }
}

std::vector<GURL> Configurator::PingUrl() const { return UpdateUrl(); }

std::string Configurator::GetProdId() const { return "cobalt"; }

base::Version Configurator::GetBrowserVersion() const {
  std::string version(COBALT_VERSION);
  // base::Version only accepts numeric versions. Return Cobalt major version.
  int first_dot = version.find_first_of(".");
  if (first_dot != std::string::npos) {
    return base::Version(version.substr(0, first_dot));
  }
  return base::Version("");
}

std::string Configurator::GetBrand() const { return {}; }

std::string Configurator::GetLang() const {
  const char* locale_id = SbSystemGetLocaleId();
  if (!locale_id) {
    // If Starboard failed to determine the locale, return a hardcoded, but
    // valid BCP 47 language tag as required by
    // https://html.spec.whatwg.org/commit-snapshots/e2f08b4e56d9a098038fb16c7ff6bb820a57cfab/#language-preferences
    return "en-US";
  }
  std::string locale_string(locale_id);
  // POSIX platforms put time zone id at the end of the locale id, like
  // |en_US.UTF8|. We remove the time zone id.
  int first_dot = locale_string.find_first_of(".");
  if (first_dot != std::string::npos) {
    return locale_string.substr(0, first_dot);
  }
  return locale_string;
}

std::string Configurator::GetOSLongName() const {
  return GetDeviceProperty(kSbSystemPropertyPlatformName);
}

base::flat_map<std::string, std::string> Configurator::ExtraRequestParams()
    const {
  base::flat_map<std::string, std::string> params;
  params.insert(std::make_pair("SABI", SB_SABI_JSON_ID));
  params.insert(std::make_pair("sbversion", std::to_string(SB_API_VERSION)));
  params.insert(
      std::make_pair("jsengine", script::GetJavaScriptEngineNameAndVersion()));
  params.insert(std::make_pair(
      "updaterchannelchanged",
      SbAtomicNoBarrier_Load(&is_channel_changed_) == 1 ? "True" : "False"));
  // Brand name
  params.insert(
      std::make_pair("brand", GetDeviceProperty(kSbSystemPropertyBrandName)));

  // Model name
  params.insert(
      std::make_pair("model", GetDeviceProperty(kSbSystemPropertyModelName)));

  // Original Design manufacturer name
  params.insert(
      std::make_pair("manufacturer",
                     GetDeviceProperty(kSbSystemPropertySystemIntegratorName)));

  // Chipset model number
  params.insert(std::make_pair(
      "chipset", GetDeviceProperty(kSbSystemPropertyChipsetModelNumber)));

  // Firmware version
  params.insert(std::make_pair(
      "firmware", GetDeviceProperty(kSbSystemPropertyFirmwareVersion)));

  // Model year
  params.insert(
      std::make_pair("year", GetDeviceProperty(kSbSystemPropertyModelYear)));

  // User Agent String
  params.insert(std::make_pair("uastring", user_agent_string_));

  // Certification scope
  params.insert(std::make_pair(
      "certscope", GetDeviceProperty(kSbSystemPropertyCertificationScope)));

  // Compression status
  params.insert(std::make_pair("usecompressedupdates",
                               GetUseCompressedUpdates() ? "True" : "False"));

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

std::string Configurator::GetAppGuidHelper(const std::string& version) {
  if (version.find(".lts.") != std::string::npos &&
      version.find(".master.") == std::string::npos) {
    return kOmahaCobaltAppID;
  }
  return kOmahaCobaltTrunkAppID;
}

std::string Configurator::GetAppGuid() const {
  const std::string version(COBALT_VERSION);
  return GetAppGuidHelper(version);
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
Configurator::GetProtocolHandlerFactory() const {
  return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
}

update_client::RecoveryCRXElevator Configurator::GetRecoveryCRXElevator()
    const {
  return {};
}

void Configurator::CompareAndSwapChannelChanged(int old_value, int new_value) {
  SbAtomicNoBarrier_CompareAndSwap(&is_channel_changed_, old_value, new_value);
}

// The updater channel is get and set by main web module thread and update
// client thread. The getter and set use a lock to prevent synchronization
// issue.
std::string Configurator::GetChannel() const {
  base::AutoLock auto_lock(const_cast<base::Lock&>(updater_channel_lock_));
  return updater_channel_;
}

void Configurator::SetChannel(const std::string& updater_channel) {
  LOG(INFO) << "Configurator::SetChannel updater_channel=" << updater_channel;
  base::AutoLock auto_lock(updater_channel_lock_);
  updater_channel_ = updater_channel;
}

// The updater status is get by main web module thread and set by the updater
// thread. The getter and set use a lock to prevent synchronization issue.
std::string Configurator::GetUpdaterStatus() const {
  base::AutoLock auto_lock(const_cast<base::Lock&>(updater_status_lock_));
  return updater_status_;
}

void Configurator::SetUpdaterStatus(const std::string& status) {
  base::AutoLock auto_lock(updater_status_lock_);
  updater_status_ = status;
}

void Configurator::SetMinFreeSpaceBytes(uint64_t bytes) {
  base::AutoLock auto_lock(const_cast<base::Lock&>(min_free_space_bytes_lock_));
  min_free_space_bytes_ = bytes;
}

uint64_t Configurator::GetMinFreeSpaceBytes() {
  base::AutoLock auto_lock(const_cast<base::Lock&>(min_free_space_bytes_lock_));
  return min_free_space_bytes_;
}

std::string Configurator::GetPreviousUpdaterStatus() const {
  base::AutoLock auto_lock(
      const_cast<base::Lock&>(previous_updater_status_lock_));
  return previous_updater_status_;
}

void Configurator::SetPreviousUpdaterStatus(const std::string& status) {
  base::AutoLock auto_lock(previous_updater_status_lock_);
  previous_updater_status_ = status;
}

bool Configurator::GetUseCompressedUpdates() const {
  return use_compressed_updates_.load();
}

void Configurator::SetUseCompressedUpdates(bool use_compressed_updates) {
  use_compressed_updates_.store(use_compressed_updates);
}

}  // namespace updater
}  // namespace cobalt
