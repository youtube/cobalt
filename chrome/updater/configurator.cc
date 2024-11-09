// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/configurator.h"

#include <regex>
#include <set>
#include <utility>
#include "base/command_line.h"
#include "base/version.h"
#include "chrome/updater/network_fetcher.h"
#include "chrome/updater/patcher.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/unzipper.h"
#include "chrome/updater/updater_constants.h"
#include "cobalt/browser/switches.h"
#include "cobalt/script/javascript_engine.h"
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

// Map of Omaha config IDs with channel and starboard version as indices.
static const std::unordered_map<std::string, std::string> kChannelAndSbVersionToOmahaIdMap = {
    {"control14", "{5F96FC23-F232-40B6-B283-FAD8DB21E1A7}"},
    {"control15", "{409F15C9-4E10-4224-9DD1-BEE7E5A2A55B}"},
    {"control16", "{30061C09-D926-4B82-8D42-600C06B6134C}"},
    {"experiment14", "{9BCC272B-3F78-43FD-9B34-A3810AE1B85D}"},
    {"experiment15", "{C412A665-9BAD-4981-93C0-264233720222}"},
    {"experiment16", "{32B5CF5A-96A4-4F64-AD0E-7C62705222FF}"},
    {"prod14", "{B3F9BCA2-8AD1-448F-9829-BB9F432815DE}"},
    {"prod15", "{14ED1D09-DAD2-4FB2-A89B-3ADC82137BCD}"},
    {"prod16", "{10F11416-0D9C-4CB1-A82A-0168594E8256}"},
    {"qa14","{94C5D27F-5981-46E8-BD4F-4645DBB5AFD3}"},
    {"qa15", "{14ED1D09-DAD2-4FB2-A89B-3ADC82137BCD}"},
    {"qa16", "{B725A22D-553A-49DC-BD61-F042B07C6B22}"},
    {"rollback14", "{A83768A9-9556-48C2-8D5E-D7C845C16C19}"},
    {"rollback15", "{1526831E-B130-43C1-B31A-AEE6E90063ED}"},
    {"rollback16", "{2A1FCBE4-E4F9-4DC3-9E1A-700FBC1D551B}"},
    {"static14", "{8001FE2C-F523-4AEC-A753-887B5CC35EBB}"},
    {"static15", "{F58B7C5F-8DC5-4E32-8CF1-455F1302D609}"},
    {"static16", "{A68BE0E4-7EE3-451A-9395-A789518FF7C5}"},
    {"t1app14", "{7C8BCB72-705D-41A6-8189-D8312E795302}"},
    {"t1app15", "{12A94004-F661-42A7-9DFC-325A4DA72D29}"},
    {"t1app16", "{B677F645-A8DB-4014-BD95-C6C06715C7CE}"},
    {"tcrash14", "{328C380E-75B4-4D7A-BF98-9B443ABA8FFF}"},
    {"tcrash15", "{1A924F1C-A46D-4104-8CCE-E8DB3C6E0ED1}"},
    {"tcrash16", "{0F876BD6-7C15-4B09-8C95-9FBBA2F93E94}"},
    {"test14", "{DB2CC00C-4FA9-4DB8-A947-647CEDAEBF29}"},
    {"test15", "{24A8A3BF-5944-4D5C-A5C4-BE00DF0FB9E1}"},
    {"test16", "{5EADD81E-A98E-4F8B-BFAA-875509A51991}"},
    {"tfailv14", "{F06C3516-8706-4579-9493-881F84606E98}"},
    {"tfailv15", "{36CFD9A7-1A73-4E8A-AC05-E3013CC4F75C}"},
    {"tfailv16", "{8F9EC6E9-B89D-4C75-9E7E-A5B9B0254844}"},
    {"tmsabi14", "{87EDA0A7-ED13-4A72-9CD9-EBD4BED081AB}"},
    {"tmsabi15", "{60296D57-2572-4B16-89EC-C9DA5A558E8A}"},
    {"tmsabi16", "{1B915523-8ADD-4C66-9E8F-D73FB48A4296}"},
    {"tnoop14", "{C407CF3F-21A4-47AA-9667-63E7EEA750CB}"},
    {"tnoop15", "{FC568D82-E608-4F15-95F0-A539AB3F6E9D}"},
    {"tnoop16", "{5F4E8AD9-067B-443A-8B63-A7CC4C95B264}"},
    {"tseries114", "{0A8A3F51-3FAB-4427-848E-28590DB75AA1}"},
    {"tseries115", "{92B7AC78-1B3B-4CE6-BA39-E1F50C4F5F72}"},
    {"tseries116", "{6E7C6582-3DC4-4B48-97F2-FA43614B2B4D}"},
    {"tseries214", "{7CB65840-5FA4-4706-BC9E-86A89A56B4E0}"},
    {"tseries215", "{7CCA7DB3-C27D-4CEB-B6A5-50A4D6DE40DA}"},
    {"tseries216", "{012BF4F5-8463-490F-B6C8-E9B64D972152}"},
};
// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;
const char kDefaultUpdaterChannel[] = "prod";
const char kOmahaCobaltAppID[] = "{6D4E53F3-CC64-4CB8-B6BD-AB0B8F300E1C}";
const char kOmahaCobaltTrunkAppID[] = "{A9557415-DDCD-4948-8113-C643EFCF710C}";
const char kOmahaCobaltLTSNightlyAppID[] =
    "{26CD2F67-091F-4680-A9A9-2229635B65A5}";
const char kOmahaCobaltProdSb14AppID[] = "{B3F9BCA2-8AD1-448F-9829-BB9F432815DE}";
const char kOmahaCobaltProdSb15AppID[] = "{14ED1D09-DAD2-4FB2-A89B-3ADC82137BCD}";
const char kOmahaCobaltProdSb16AppID[] = "{10F11416-0D9C-4CB1-A82A-0168594E8256}";

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
  const std::string persisted_channel = persisted_data_->GetLatestChannel();
  if (persisted_channel.empty()) {
    SetChannel(kDefaultUpdaterChannel);
  } else {
    SetChannel(persisted_channel);
  }
  if (network_module != nullptr) {
    user_agent_string_ = network_module->GetUserAgent();
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          browser::switches::kUseUncompressedUpdates)) {
    use_compressed_updates_.store(false);
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 browser::switches::kLoaderUseMemoryMappedFile)) {
    // Devices with a loader app built prior to 25 LTS do not have
    // kUseUncompressedUpdates available to them to disable compressed updates.
    // Since binary compression and the Memory Mapped file feature are
    // incompatible, we need to make sure that these devices do not request
    // compressed updates if they are using the Memory Mapped file feature.
    use_compressed_updates_.store(false);
    LOG(INFO) << "Uncompressed updates will be requested since the Memory "
                << "Mapped file feature is used";
  } else {
    use_compressed_updates_.store(true);
  }
}
Configurator::~Configurator() { LOG(INFO) << "Configurator::~Configurator"; }

int Configurator::InitialDelay() const {
  return 0;
}

int Configurator::NextCheckDelay() const {
  return 5 * kDelayOneHour;
}

int Configurator::OnDemandDelay() const {
  return 0;
}

int Configurator::UpdateDelay() const {
  return 0;
}

std::vector<GURL> Configurator::UpdateUrl() const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          browser::switches::kUseQAUpdateServer)) {
    return std::vector<GURL>{GURL(kUpdaterJSONDefaultUrlQA)};
  } else {
  return std::vector<GURL>{GURL(kUpdaterJSONDefaultUrl)};
  }
}

std::vector<GURL> Configurator::PingUrl() const {
  return UpdateUrl();
}

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

std::string Configurator::GetDownloadPreference() const {
  return {};
}

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

bool Configurator::EnabledDeltas() const {
  return false;
}

bool Configurator::EnabledComponentUpdates() const {
  return false;
}

bool Configurator::EnabledBackgroundDownloader() const {
  return false;
}

// TODO: enable cup signing
bool Configurator::EnabledCupSigning() const { return false; }

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

std::vector<uint8_t> Configurator::GetRunActionKeyHash() const {
  return {};
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
  if (std::regex_match(updater_channel, std::regex("2[5-9]lts\\d+"))) {
    channel = "static";
  }
  auto it = kChannelAndSbVersionToOmahaIdMap.find(channel + std::to_string(sb_version));
  if (it != kChannelAndSbVersionToOmahaIdMap.end()) {
    return it->second;
  }
  LOG(INFO) << "Configurator::GetAppGuidHelper updater channel and starboard combination is invalid.";
  
  // All invalid channel requests get updates from prod config.
  if (sb_version == 14) {
    return kOmahaCobaltProdSb14AppID;
  }
  if (sb_version == 15) {
    return kOmahaCobaltProdSb15AppID;
  }
  if (sb_version == 16) {
    return kOmahaCobaltProdSb16AppID;
  }
  // Should not reach here.
  LOG(INFO) << "Configurator::GetAppGuidHelper starboard version is invalid.";
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
