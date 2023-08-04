// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "cobalt/network/network_module.h"
#include "components/update_client/configurator.h"
#include "components/update_client/persisted_data.h"
#include "starboard/common/atomic.h"

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

class Configurator : public update_client::Configurator {
 public:
  explicit Configurator(network::NetworkModule* network_module);

  // Configurator for update_client::Configurator.
  int InitialDelay() const override;
  int NextCheckDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
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
  scoped_refptr<update_client::UnzipperFactory> GetUnzipperFactory() override;
  scoped_refptr<update_client::PatcherFactory> GetPatcherFactory() override;
  bool EnabledDeltas() const override;
  bool EnabledComponentUpdates() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  update_client::ActivityDataService* GetActivityDataService() const override;
  bool IsPerUserInstall() const override;
  std::vector<uint8_t> GetRunActionKeyHash() const override;
  std::string GetAppGuid() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  update_client::RecoveryCRXElevator GetRecoveryCRXElevator() const override;

  void SetChannel(const std::string& updater_channel) override;

  void CompareAndSwapChannelChanged(int old_value, int new_value) override;

  std::string GetUpdaterStatus() const override;
  void SetUpdaterStatus(const std::string& status) override;

  std::string GetPreviousUpdaterStatus() const override;
  void SetPreviousUpdaterStatus(const std::string& status) override;

  void SetMinFreeSpaceBytes(uint64_t bytes) override;
  uint64_t GetMinFreeSpaceBytes() override;

  bool GetUseCompressedUpdates() const override;
  void SetUseCompressedUpdates(bool use_compressed_updates) override;
  // Added for testing purposes.
  static std::string GetAppGuidHelper(const std::string& updater_channel,
                                      const std::string& version);

 private:
  friend class base::RefCountedThreadSafe<Configurator>;
  ~Configurator() override;

  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<update_client::PersistedData> persisted_data_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::UnzipperFactory> unzip_factory_;
  scoped_refptr<update_client::PatcherFactory> patch_factory_;
  std::string updater_channel_;
  base::Lock updater_channel_lock_;
  SbAtomic32 is_channel_changed_;
  std::string updater_status_;
  base::Lock updater_status_lock_;
  std::string previous_updater_status_;
  base::Lock previous_updater_status_lock_;
  std::string user_agent_string_;
  uint64_t min_free_space_bytes_ = 48 * 1024 * 1024;
  base::Lock min_free_space_bytes_lock_;
  starboard::atomic_bool use_compressed_updates_;

  DISALLOW_COPY_AND_ASSIGN(Configurator);
};

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_CONFIGURATOR_H_
