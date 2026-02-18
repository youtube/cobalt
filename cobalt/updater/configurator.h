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

#ifndef COBALT_UPDATER_CONFIGURATOR_H_
#define COBALT_UPDATER_CONFIGURATOR_H_

#include <stdint.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/update_client/configurator.h"
#include "components/update_client/persisted_data.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class GURL;
class PrefService;

namespace base {
class Version;
}  // namespace base

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace update_client {
class ActivityDataService;
class NetworkFetcherFactory;
class ProtocolHandlerFactory;
}  // namespace update_client

namespace cobalt {
namespace updater {
// Configurator class for the Cobalt updater.
// It implements the update_client::Configurator interface and provides
// Cobalt-specific configuration parameters for the update process.
//
// Provides Cobalt-specific configurations to the update client, such as update
// URLs, version information, and other settings required for the update
// process.
// Exists for the duration of the application's runtime, destroyed when app is
// suspended or exits.
// TODO: (b/451696797) Adding a thread_checker for each TaskRunner accessing
// the Configurator instance.
// Accessed by both the updater thread and the main thread. Synchronization
// mechanisms (locks, atomics) are used to ensure thread safety.
class Configurator : public update_client::Configurator {
 public:
  explicit Configurator(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& user_agent);

  // Configurator for update_client::Configurator.
  base::TimeDelta InitialDelay() const override;
  base::TimeDelta NextCheckDelay() const override;
  base::TimeDelta OnDemandDelay() const override;
  base::TimeDelta UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetBrand() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  base::flat_map<std::string, std::string> ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  scoped_refptr<update_client::NetworkFetcherFactory> GetNetworkFetcherFactory()
      override;
  scoped_refptr<update_client::CrxDownloaderFactory> GetCrxDownloaderFactory()
      override;
  scoped_refptr<update_client::UnzipperFactory> GetUnzipperFactory() override;
  scoped_refptr<update_client::PatcherFactory> GetPatcherFactory() override;
  bool EnabledDeltas() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  update_client::ActivityDataService* GetActivityDataService() const override;
  absl::optional<bool> IsMachineExternallyManaged() const override;
  bool IsPerUserInstall() const override;
  std::string GetAppGuid() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  update_client::UpdaterStateProvider GetUpdaterStateProvider() const override;

  void SetChannel(const std::string& updater_channel) override;

  void CompareAndSwapForcedUpdate(int old_value, int new_value) override;

  std::string GetUpdaterStatus() const override;
  void SetUpdaterStatus(const std::string& status) override;

  std::string GetPreviousUpdaterStatus() const override;
  void SetPreviousUpdaterStatus(const std::string& status) override;

  void SetMinFreeSpaceBytes(uint64_t bytes) override;
  uint64_t GetMinFreeSpaceBytes() override;

  bool GetUseCompressedUpdates() const override;
  void SetUseCompressedUpdates(bool use_compressed_updates) override;
  static std::string GetAppGuidHelper(const std::string& updater_channel,
                                      const std::string& version,
                                      const int sb_version);

  bool GetAllowSelfSignedPackages() const override;
  void SetAllowSelfSignedPackages(bool allow_self_signed_packages) override;

  std::string GetUpdateServerUrl() const override;
  void SetUpdateServerUrl(const std::string& update_server_url) override;

  bool GetRequireNetworkEncryption() const override;
  void SetRequireNetworkEncryption(bool require_network_encryption) override;

 private:
  friend class base::RefCountedThreadSafe<Configurator>;
  ~Configurator() override;

  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<update_client::PersistedData> persisted_data_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::CrxDownloaderFactory> crx_downloader_factory_;
  scoped_refptr<update_client::UnzipperFactory> unzip_factory_;
  scoped_refptr<update_client::PatcherFactory> patch_factory_;
  // TODO(b/449220798): Consider using PostTask and thread checker
  std::string updater_channel_ GUARDED_BY(updater_channel_lock_);
  base::Lock updater_channel_lock_;
  std::atomic<int32_t> is_forced_update_;
  std::string updater_status_ GUARDED_BY(updater_status_lock_);
  base::Lock updater_status_lock_;
  std::string previous_updater_status_
      GUARDED_BY(previous_updater_status_lock_);
  base::Lock previous_updater_status_lock_;
  std::string user_agent_string_;
  uint64_t min_free_space_bytes_ GUARDED_BY(min_free_space_bytes_lock_);
  base::Lock min_free_space_bytes_lock_;
  std::atomic_bool use_compressed_updates_;
  std::atomic_bool allow_self_signed_packages_;
  std::string update_server_url_ GUARDED_BY(update_server_url_lock_);
  base::Lock update_server_url_lock_;
  std::atomic_bool require_network_encryption_;
};

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_CONFIGURATOR_H_
