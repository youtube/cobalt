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

#ifndef COBALT_UPDATER_UPDATER_MODULE_H_
#define COBALT_UPDATER_UPDATER_MODULE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "cobalt/network/network_module.h"
#include "cobalt/updater/configurator.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "starboard/event.h"

namespace cobalt {
namespace updater {

// An interface that observes the updater. It provides notifications when the
// updater changes status.
class Observer : public update_client::UpdateClient::Observer {
 public:
  Observer(scoped_refptr<update_client::UpdateClient> update_client,
           scoped_refptr<Configurator> updater_configurator)
      : update_client_(update_client),
        updater_configurator_(updater_configurator) {}

  // Overrides for update_client::UpdateClient::Observer.
  void OnEvent(Events event, const std::string& id) override;

 private:
  scoped_refptr<update_client::UpdateClient> update_client_;
  scoped_refptr<Configurator> updater_configurator_;
  update_client::CrxUpdateItem crx_update_item_;
  DISALLOW_COPY_AND_ASSIGN(Observer);
};

// UpdaterModule checks for available Cobalt update and downloads the update
// that matches the underlying Starboard ABI (SABI) configuration. Then
// the update is unpacked to a dedicated location for installation. The update
// checks run according to a schedule defined by the Cobalt application.
class UpdaterModule {
 public:
  explicit UpdaterModule(network::NetworkModule* network_module);
  ~UpdaterModule();

  void Suspend();
  void Resume();

  std::string GetUpdaterChannel() const;
  void SetUpdaterChannel(const std::string& updater_channel);

  void MarkChannelChanged() { updater_configurator_->MarkChannelChanged(); }
  bool IsChannelChanged() { return updater_configurator_->IsChannelChanged(); }
  bool IsChannelValid(const std::string& channel) {
    return updater_configurator_->IsChannelValid(channel);
  }

  void RunUpdateCheck();

  std::string GetUpdaterStatus() const {
    return updater_configurator_->GetUpdaterStatus();
  }

  void ResetInstallations();

 private:
  base::Thread updater_thread_;
  scoped_refptr<update_client::UpdateClient> update_client_;
  std::unique_ptr<Observer> updater_observer_;
  network::NetworkModule* network_module_;
  scoped_refptr<Configurator> updater_configurator_;
  int update_check_count_ = 0;
  bool is_updater_running_;

  THREAD_CHECKER(thread_checker_);

  int GetUpdateCheckCount() { return update_check_count_; }
  void IncrementUpdateCheckCount() { update_check_count_++; }

  void Initialize();
  void Finalize();
  void MarkSuccessful();
  void Update();
};

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_UPDATER_MODULE_H_
