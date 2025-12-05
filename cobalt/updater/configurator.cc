// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/updater/configurator.h"

#include <regex>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/version.h"
#include "cobalt/browser/switches.h"
#include "cobalt/updater/network_fetcher.h"
#include "cobalt/updater/patcher.h"
#include "cobalt/updater/prefs.h"
#include "cobalt/updater/unzipper.h"
#include "cobalt/updater/util.h"
#include "cobalt/version.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/network.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzipper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "starboard/common/system_property.h"
#include "starboard/system.h"
#include "url/gurl.h"

using starboard::kSystemPropertyMaxLength;

namespace {

const char kDefaultUpdaterChannel[] = "prod";
const char kUpdaterJSONDefaultUrlQA[] =
    "https://omaha-qa.sandbox.google.com/service/update2/json";
const char kUpdaterJSONDefaultUrl[] =
    "https://tools.google.com/service/update2/json";

std::string GetDeviceProperty(SbSystemPropertyId id) {
  char value[kSystemPropertyMaxLength];
  if (SbSystemGetProperty(id, value, kSystemPropertyMaxLength)) {
    return std::string(value);
  }
  return std::string();
}

}  // namespace

namespace cobalt {
namespace updater {

Configurator::Configurator(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pref_service_(CreatePrefService()),
      persisted_data_(
          std::make_unique<update_client::PersistedData>(pref_service_.get(),
                                                         nullptr)),
      network_fetcher_factory_(
          base::MakeRefCounted<NetworkFetcherFactoryCobalt>(
              url_loader_factory)),
      crx_downloader_factory_(
          update_client::MakeCrxDownloaderFactory(network_fetcher_factory_)),
      unzip_factory_(base::MakeRefCounted<UnzipperFactory>()),
      patch_factory_(base::MakeRefCounted<PatcherFactory>()),
      is_forced_update_(0),
      min_free_space_bytes_(48 * 1024 * 1024),  // 48MB
      allow_self_signed_packages_(false),
      require_network_encryption_(true) {
  LOG(INFO) << "Configurator::Configurator";

  const std::string persisted_channel = persisted_data_->GetLatestChannel();
  if (persisted_channel.empty()) {
    SetChannel(kDefaultUpdaterChannel);
  } else {
    SetChannel(persisted_channel);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseUncompressedUpdates)) {
    use_compressed_updates_.store(false);
  } else {
    use_compressed_updates_.store(true);
  }
}
Configurator::~Configurator() {
  LOG(INFO) << "Configurator::~Configurator";
}

base::TimeDelta Configurator::InitialDelay() const {
  return base::Seconds(0);
}

base::TimeDelta Configurator::NextCheckDelay() const {
  return base::Hours(5);
}

base::TimeDelta Configurator::OnDemandDelay() const {
  return base::Seconds(0);
}

base::TimeDelta Configurator::UpdateDelay() const {
  return base::Seconds(0);
}

std::vector<GURL> Configurator::UpdateUrl() const {
#if !defined(COBALT_BUILD_TYPE_GOLD)
  if (allow_self_signed_packages_ && !update_server_url_.empty()) {
    return std::vector<GURL>{GURL(update_server_url_)};
  }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseQAUpdateServer)) {
    return std::vector<GURL>{GURL(kUpdaterJSONDefaultUrlQA)};
  } else {
    return std::vector<GURL>{GURL(kUpdaterJSONDefaultUrl)};
  }
}

std::vector<GURL> Configurator::PingUrl() const {
  return UpdateUrl();
}

std::string Configurator::GetProdId() const {
  return "cobalt";
}

base::Version Configurator::GetBrowserVersion() const {
  return base::Version(COBALT_MAJOR_VERSION);
}

std::string Configurator::GetBrand() const {
  return {};
}

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

  // The flag name to force an update is changed to `is_forced_update_` to
  // cover the cases where update is requested by client but channel stays the
  // same, but the flag name sent to Omaha remains `updaterchannelchanged` to
  // isolate the changes to Omaha configs. If it's decided to change this flag
  // name on Omaha, it'll be done in a separate PR after the Omaha configs are
  // updated.
  params.insert(std::make_pair(
      "updaterchannelchanged",
      std::atomic_load(&is_forced_update_) == 1 ? "True" : "False"));
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

std::string Configurator::GetDownloadPreference() const {
  return {};
}

// TODO(b/449022872): consider snake case names
scoped_refptr<update_client::NetworkFetcherFactory>
Configurator::GetNetworkFetcherFactory() {
  return network_fetcher_factory_;
}

scoped_refptr<update_client::CrxDownloaderFactory>
Configurator::GetCrxDownloaderFactory() {
  return crx_downloader_factory_;
}

scoped_refptr<update_client::UnzipperFactory>
Configurator::GetUnzipperFactory() {
  return unzip_factory_;
}

scoped_refptr<update_client::PatcherFactory> Configurator::GetPatcherFactory() {
  return patch_factory_;
}

bool Configurator::EnabledDeltas() const {
  return false;
}

bool Configurator::EnabledBackgroundDownloader() const {
  return false;
}

bool Configurator::EnabledCupSigning() const {
  return false;
}

PrefService* Configurator::GetPrefService() const {
  return pref_service_.get();
}

update_client::ActivityDataService* Configurator::GetActivityDataService()
    const {
  return nullptr;
}

bool Configurator::IsPerUserInstall() const {
  return true;
}

absl::optional<bool> Configurator::IsMachineExternallyManaged() const {
  return false;
}

update_client::UpdaterStateProvider Configurator::GetUpdaterStateProvider()
    const {
  return base::BindRepeating([](bool /*is_machine*/) {
    return update_client::UpdaterStateAttributes();
  });
}

std::string Configurator::GetAppGuidHelper(const std::string& updater_channel,
                                           const std::string& version,
                                           const int sb_version) {
  if (updater_channel == "ltsnightly" || updater_channel == "ltsnightlyqa") {
    return kOmahaCobaltLTSNightlyAppID;
  }
  if (version.find(".lts.") == std::string::npos &&
      version.find(".master.") != std::string::npos) {
    return kOmahaCobaltTrunkAppID;
  }
  std::string channel(updater_channel);
  // This regex matches to all static channels for C25 and newer in the format
  // of XXltsY.
  if (std::regex_match(updater_channel,
                       std::regex("(2[5-9]|[3-9][0-9])lts\\d+"))) {
    channel = "static";
  }
  auto it = kChannelAndSbVersionToOmahaIdMap.find(channel +
                                                  std::to_string(sb_version));
  if (it != kChannelAndSbVersionToOmahaIdMap.end()) {
    return it->second;
  }
  LOG(INFO) << "Configurator::GetAppGuidHelper updater channel and starboard "
            << "combination is undefined with the new Omaha configs.";

  // All undefined channel requests go to prod configs except for static
  // channel requestsf for C24 and older.
  // TODO(b/449024263): Replace regex matchers with substring_set_matcher or re2
  if (!std::regex_match(updater_channel, std::regex("2[0-4]lts\\d+")) &&
      sb_version >= 14 && sb_version <= 16) {
    const auto it = kChannelAndSbVersionToOmahaIdMap.find(
        "prod" + std::to_string(sb_version));
    if (it != kChannelAndSbVersionToOmahaIdMap.end()) {
      return it->second;
    }
  }
  LOG(INFO) << __func__ << " starboard version is invalid.";
  return kOmahaCobaltAppID;
}

std::string Configurator::GetAppGuid() const {
  const std::string version(COBALT_VERSION);
  return GetAppGuidHelper(updater_channel_, version, SB_API_VERSION);
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
Configurator::GetProtocolHandlerFactory() const {
  return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
}

void Configurator::CompareAndSwapForcedUpdate(int old_value, int new_value) {
  is_forced_update_.compare_exchange_weak(old_value, new_value);
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

bool Configurator::GetAllowSelfSignedPackages() const {
  return allow_self_signed_packages_.load();
}

void Configurator::SetAllowSelfSignedPackages(bool allow_self_signed_packages) {
  allow_self_signed_packages_.store(allow_self_signed_packages);
}

std::string Configurator::GetUpdateServerUrl() const {
  base::AutoLock auto_lock(const_cast<base::Lock&>(update_server_url_lock_));
  return update_server_url_;
}

void Configurator::SetUpdateServerUrl(const std::string& update_server_url) {
  LOG(INFO) << "Configurator::SetUpdateServerUrl update_server_url="
            << update_server_url;
  base::AutoLock auto_lock(update_server_url_lock_);
  update_server_url_ = update_server_url;
}

bool Configurator::GetRequireNetworkEncryption() const {
  return require_network_encryption_.load();
}
void Configurator::SetRequireNetworkEncryption(
    bool require_network_encryption) {
  require_network_encryption_.store(require_network_encryption);
}

}  // namespace updater
}  // namespace cobalt
