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

#ifndef COBALT_UPDATER_UPDATER_MODULE_H_
#define COBALT_UPDATER_UPDATER_MODULE_H_

#include <map>
#include <memory>
#include <string>

#include "base/no_destructor.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "cobalt/updater/configurator.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "starboard/event.h"
#include "starboard/extension/updater_notification.h"

// TODO(b/449223391): Revist and document the usage of base::Unretained()

namespace cobalt {
namespace updater {

// Default delay the first update check.
extern const base::TimeDelta kDefaultUpdateCheckDelay;

// An interface that observes the updater. It provides notifications when the
// updater changes status.
class Observer : public update_client::UpdateClient::Observer {
 public:
  Observer(scoped_refptr<update_client::UpdateClient> update_client,
           scoped_refptr<Configurator> updater_configurator)
      : update_client_(update_client),
        updater_configurator_(updater_configurator) {
    const CobaltExtensionUpdaterNotificationApi* updater_notification_ext =
        static_cast<const CobaltExtensionUpdaterNotificationApi*>(
            SbSystemGetExtension(kCobaltExtensionUpdaterNotificationName));
    if (updater_notification_ext &&
        strcmp(updater_notification_ext->name,
               kCobaltExtensionUpdaterNotificationName) == 0 &&
        updater_notification_ext->version >= 1) {
      updater_notification_ext_ = updater_notification_ext;
    } else {
      updater_notification_ext_ = nullptr;
    }
  }

  // Overrides for update_client::UpdateClient::Observer.
  void OnEvent(Events event, const std::string& id) override;

 private:
  scoped_refptr<update_client::UpdateClient> update_client_;
  scoped_refptr<Configurator> updater_configurator_;
  update_client::CrxUpdateItem crx_update_item_;
  const CobaltExtensionUpdaterNotificationApi* updater_notification_ext_;
};

// UpdaterModule checks for available Cobalt update and downloads the update
// that matches the underlying Starboard ABI (SABI) configuration. Then
// the update is unpacked to a dedicated location for installation. The update
// checks run according to a schedule defined by the Cobalt application.
//
// Manages the update process for Cobalt, including checking for updates,
// downloading them, and preparing them for installation.
// Exists for the duration of the application's runtime.
// The update checks and downloads are performed on a dedicated updater
// thread.
class UpdaterModule {
 public:
  static void CreateInstance(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::TimeDelta update_check_delay);

  static UpdaterModule* GetInstance();

  UpdaterModule(const UpdaterModule&) = delete;
  UpdaterModule& operator=(const UpdaterModule&) = delete;

  void Suspend();
  void Resume();

  std::string GetUpdaterChannel() const;
  void SetUpdaterChannel(const std::string& updater_channel);

  void CompareAndSwapForcedUpdate(int old_value, int new_value);

  void RunUpdateCheck();

  std::string GetUpdaterStatus() const;

  void ResetInstallations();

  int GetInstallationIndex() const;

  void SetMinFreeSpaceBytes(uint64_t bytes);

  bool GetUseCompressedUpdates() const;
  void SetUseCompressedUpdates(bool use_compressed_updates);

  bool GetAllowSelfSignedPackages() const;
  void SetAllowSelfSignedPackages(bool allow_self_signed_packages);

  std::string GetUpdateServerUrl() const;
  void SetUpdateServerUrl(const std::string& update_server_url);

  bool GetRequireNetworkEncryption() const;
  void SetRequireNetworkEncryption(bool require_network_encryption);

  void MarkSuccessful();

 private:
  // Private constructor and destructor to enforce singleton pattern.
  explicit UpdaterModule(scoped_refptr<network::SharedURLLoaderFactory>,
                         base::TimeDelta update_check_delay);
  ~UpdaterModule();

  // TODO: b/454440974 Investigate whether singleton is necessary.
  friend class base::NoDestructor<UpdaterModule>;

  std::unique_ptr<base::Thread> updater_thread_;
  scoped_refptr<update_client::UpdateClient> update_client_;
  std::unique_ptr<Observer> updater_observer_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  scoped_refptr<Configurator> updater_configurator_;
  int update_check_count_ = 0;
  bool is_updater_running_;
  base::TimeDelta update_check_delay_ = kDefaultUpdateCheckDelay;

  int GetUpdateCheckCount() { return update_check_count_; }
  void IncrementUpdateCheckCount() { update_check_count_++; }

  void Initialize(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory);
  void Finalize();
  void MarkSuccessfulImpl();
  void Update();

  // Holds the single instance of UpdaterModule.
  static base::NoDestructor<UpdaterModule>* updater_module_;
};

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_UPDATER_MODULE_H_
