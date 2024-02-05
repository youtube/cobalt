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

#include "cobalt/updater/updater_module.h"

#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "cobalt/browser/switches.h"
#include "cobalt/updater/crash_client.h"
#include "cobalt/updater/crash_reporter.h"
#include "cobalt/updater/utils.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/cobalt_slot_management.h"
#include "components/update_client/utils.h"
#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/installation_manager.h"

namespace {

using update_client::CobaltSlotManagement;
using update_client::ComponentState;

// The SHA256 hash of the "cobalt_evergreen_public" key.
constexpr uint8_t kCobaltPublicKeyHash[] = {
    0x51, 0xa8, 0xc0, 0x90, 0xf8, 0x1a, 0x14, 0xb0, 0xda, 0x7a, 0xfb,
    0x9e, 0x8b, 0x2d, 0x22, 0x65, 0x19, 0xb1, 0xfa, 0xba, 0x02, 0x04,
    0x3a, 0xb2, 0x7a, 0xf6, 0xfe, 0xd5, 0x35, 0xa1, 0x19, 0xd9};

void QuitLoop(base::OnceClosure quit_closure) { std::move(quit_closure).Run(); }

CobaltExtensionUpdaterNotificationState
ComponentStateToCobaltExtensionUpdaterNotificationState(
    ComponentState component_state) {
  switch (component_state) {
    case ComponentState::kChecking:
      return kCobaltExtensionUpdaterNotificationStateChecking;
    case ComponentState::kCanUpdate:
      return kCobaltExtensionUpdaterNotificationStateUpdateAvailable;
    case ComponentState::kDownloading:
      return kCobaltExtensionUpdaterNotificationStateDownloading;
    case ComponentState::kDownloaded:
      return kCobaltExtensionUpdaterNotificationStateDownloaded;
    case ComponentState::kUpdating:
      return kCobaltExtensionUpdaterNotificationStateInstalling;
#if SB_API_VERSION > 13
    case ComponentState::kUpdated:
      return kCobaltExtensionUpdaterNotificationStateUpdated;
    case ComponentState::kUpToDate:
      return kCobaltExtensionUpdaterNotificationStateUpToDate;
    case ComponentState::kUpdateError:
      return kCobaltExtensionUpdaterNotificationStateUpdateFailed;
#else
    case ComponentState::kUpdated:
      return kCobaltExtensionUpdaterNotificationStatekUpdated;
    case ComponentState::kUpToDate:
      return kCobaltExtensionUpdaterNotificationStatekUpToDate;
    case ComponentState::kUpdateError:
      return kCobaltExtensionUpdaterNotificationStatekUpdateFailed;
#endif
    default:
      return kCobaltExtensionUpdaterNotificationStateNone;
  }
}

}  // namespace

namespace cobalt {
namespace updater {

// The delay in seconds before the first update check.
const uint64_t kDefaultUpdateCheckDelaySeconds = 30;

void Observer::OnEvent(Events event, const std::string& id) {
  LOG(INFO) << "Observer::OnEvent id=" << id;
  std::string status;
  if (update_client_->GetCrxUpdateState(id, &crx_update_item_)) {
    auto status_iterator =
        component_to_updater_status_map.find(crx_update_item_.state);
    if (status_iterator == component_to_updater_status_map.end()) {
      status = "Status is unknown.";
    } else if (crx_update_item_.state == ComponentState::kUpToDate &&
               updater_configurator_->GetPreviousUpdaterStatus().compare(
                   updater_status_string_map.find(UpdaterStatus::kUpdated)
                       ->second) == 0) {
      status = std::string(
          updater_status_string_map.find(UpdaterStatus::kUpdated)->second);
    } else {
      status = std::string(
          updater_status_string_map.find(status_iterator->second)->second);
    }
    if (crx_update_item_.state == ComponentState::kUpdateError) {
      status +=
          ", error code is " + std::to_string(crx_update_item_.error_code);
    }
    if (updater_notification_ext_ != nullptr) {
      updater_notification_ext_->UpdaterState(
          ComponentStateToCobaltExtensionUpdaterNotificationState(
              crx_update_item_.state),
          GetCurrentEvergreenVersion().c_str());
    }
  } else {
    status = "No status available";
  }
  updater_configurator_->SetUpdaterStatus(status);
  LOG(INFO) << "Updater status is " << status;
}

UpdaterModule::UpdaterModule(network::NetworkModule* network_module,
                             uint64_t update_check_delay_sec)
    : network_module_(network_module),
      update_check_delay_sec_(update_check_delay_sec) {
  LOG(INFO) << "UpdaterModule::UpdaterModule";
  updater_thread_.reset(new base::Thread("Updater"));
  updater_thread_->StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  DETACH_FROM_THREAD(thread_checker_);
  // Initialize the underlying update client.
  is_updater_running_ = true;
  updater_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&UpdaterModule::Initialize, base::Unretained(this)));
}

UpdaterModule::~UpdaterModule() {
  LOG(INFO) << "UpdaterModule::~UpdaterModule";
  if (is_updater_running_) {
    is_updater_running_ = false;
    updater_thread_->task_runner()->PostBlockingTask(
        FROM_HERE,
        base::Bind(&UpdaterModule::Finalize, base::Unretained(this)));
  }

  // Upon destruction the thread will allow all queued tasks to complete before
  // the thread is terminated. The thread is destroyed before returning from
  // this destructor to prevent one of the thread's tasks from accessing member
  // fields after they are destroyed.
  updater_thread_.reset();
}

void UpdaterModule::Suspend() {
  if (is_updater_running_) {
    is_updater_running_ = false;
    updater_thread_->task_runner()->PostBlockingTask(
        FROM_HERE,
        base::Bind(&UpdaterModule::Finalize, base::Unretained(this)));
  }
}

void UpdaterModule::Resume() {
  if (!is_updater_running_) {
    is_updater_running_ = true;
    updater_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&UpdaterModule::Initialize, base::Unretained(this)));
  }
}

void UpdaterModule::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  updater_configurator_ = base::MakeRefCounted<Configurator>(network_module_);
  update_client_ = update_client::UpdateClientFactory(updater_configurator_);

  updater_observer_.reset(new Observer(update_client_, updater_configurator_));
  update_client_->AddObserver(updater_observer_.get());

  // Schedule the first update check.

  LOG(INFO) << "Scheduling UpdateCheck with delay " << update_check_delay_sec_
            << " seconds";
  updater_thread_->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&UpdaterModule::Update, base::Unretained(this)),
      base::TimeDelta::FromSeconds(update_check_delay_sec_));
}

void UpdaterModule::Finalize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(INFO) << "UpdaterModule::Finalize begin";
  update_client_->RemoveObserver(updater_observer_.get());
  updater_observer_.reset();
  update_client_->Stop();
  update_client_ = nullptr;

  if (updater_configurator_ != nullptr) {
    auto pref_service = updater_configurator_->GetPrefService();
    if (pref_service != nullptr) {
      pref_service->CommitPendingWrite(
          base::BindOnce(&QuitLoop, base::Bind(base::DoNothing::Repeatedly())));
    }
  }

  updater_configurator_ = nullptr;

  // Cleanup drain files
  const auto installation_manager =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (installation_manager) {
    CobaltSlotManagement cobalt_slot_management;
    if (cobalt_slot_management.Init(installation_manager)) {
      cobalt_slot_management.CleanupAllDrainFiles();
    }
  }
  LOG(INFO) << "UpdaterModule::Finalize end";
}

void UpdaterModule::MarkSuccessful() {
  updater_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&UpdaterModule::MarkSuccessfulImpl, base::Unretained(this)));
}

void UpdaterModule::MarkSuccessfulImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(INFO) << "UpdaterModule::MarkSuccessfulImpl";

  auto installation_manager =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_manager) {
    LOG(ERROR) << "Updater failed to get installation manager extension.";
    return;
  }
  int index = installation_manager->GetCurrentInstallationIndex();
  if (index == IM_EXT_ERROR) {
    LOG(ERROR) << "Updater failed to get current installation index.";
    return;
  }
  if (installation_manager->MarkInstallationSuccessful(index) !=
      IM_EXT_SUCCESS) {
    LOG(ERROR) << "Updater failed to mark the current installation successful";
  }
}

void UpdaterModule::Update() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If updater_configurator_ is nullptr, the updater is suspended.
  if (updater_configurator_ == nullptr) {
    return;
  }
  const std::vector<std::string> app_ids = {
      updater_configurator_->GetAppGuid()};

  const base::Version manifest_version(GetCurrentEvergreenVersion());
  if (!manifest_version.IsValid()) {
    LOG(ERROR) << "Updater failed to get the current update version.";
    return;
  }

  update_client_->Update(
      app_ids,
      base::BindOnce(
          [](base::Version manifest_version,
             const std::vector<std::string>& ids)
              -> std::vector<base::Optional<update_client::CrxComponent>> {
            update_client::CrxComponent component;
            component.name = "cobalt";
            component.app_id = ids[0];
            component.version = manifest_version;
            component.pk_hash.assign(std::begin(kCobaltPublicKeyHash),
                                     std::end(kCobaltPublicKeyHash));
            component.requires_network_encryption = true;
            component.crx_format_requirement = crx_file::VerifierFormat::CRX3;
            return {component};
          },
          manifest_version),
      false,
      base::BindOnce(
          [](base::OnceClosure closure, update_client::Error error) {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindOnce(&QuitLoop, std::move(closure)));
          },
          base::Bind(base::DoNothing::Repeatedly())));

  IncrementUpdateCheckCount();

  int kNextUpdateCheckHours = 0;
  if (GetUpdateCheckCount() == 1) {
    // Update check ran once. The next check will be scheduled at a randomized
    // time between 1 and 24 hours.
    kNextUpdateCheckHours = base::RandInt(1, 24);
  } else {
    // Update check ran at least twice. The next check will be scheduled in 24
    // hours.
    kNextUpdateCheckHours = 24;
  }
  updater_thread_->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&UpdaterModule::Update, base::Unretained(this)),
      base::TimeDelta::FromHours(kNextUpdateCheckHours));
}

// The following methods are called by other threads than the updater_thread_.

void UpdaterModule::CompareAndSwapChannelChanged(int old_value, int new_value) {
  auto config = updater_configurator_;
  if (config) config->CompareAndSwapChannelChanged(old_value, new_value);
}

std::string UpdaterModule::GetUpdaterChannel() const {
  LOG(INFO) << "UpdaterModule::GetUpdaterChannel";
  auto config = updater_configurator_;
  if (!config) {
    LOG(ERROR) << "UpdaterModule::GetUpdaterChannel: missing configurator";
    return "";
  }

  std::string channel = config->GetChannel();
  LOG(INFO) << "UpdaterModule::GetUpdaterChannel channel=" << channel;
  return channel;
}

void UpdaterModule::SetUpdaterChannel(const std::string& updater_channel) {
  LOG(INFO) << "UpdaterModule::SetUpdaterChannel updater_channel="
            << updater_channel;
  auto config = updater_configurator_;
  if (config) config->SetChannel(updater_channel);
}

std::string UpdaterModule::GetUpdaterStatus() const {
  LOG(INFO) << "UpdaterModule::GetUpdaterStatus";
  auto config = updater_configurator_;
  if (!config) {
    LOG(ERROR) << "UpdaterModule::GetUpdaterStatus: missing configurator";
    return "";
  }

  std::string updater_status = config->GetUpdaterStatus();
  LOG(INFO) << "UpdaterModule::GetUpdaterStatus updater_status="
            << updater_status;
  return updater_status;
}

void UpdaterModule::RunUpdateCheck() {
  updater_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&UpdaterModule::Update, base::Unretained(this)));
}

void UpdaterModule::ResetInstallations() {
  auto installation_manager =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_manager) {
    LOG(ERROR) << "Updater failed to get installation manager extension.";
    return;
  }
  if (installation_manager->Reset() == IM_EXT_ERROR) {
    LOG(ERROR) << "Updater failed to reset installations.";
    return;
  }
  base::FilePath product_data_dir;
  if (!GetProductDirectoryPath(&product_data_dir)) {
    LOG(ERROR) << "Updater failed to get product directory path.";
    return;
  }
  if (!starboard::SbFileDeleteRecursive(product_data_dir.value().c_str(),
                                        true)) {
    LOG(ERROR) << "Updater failed to clean the product directory.";
    return;
  }
}

int UpdaterModule::GetInstallationIndex() const {
  auto installation_manager =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_manager) {
    LOG(ERROR) << "Updater failed to get installation manager extension.";
    return -1;
  }
  int index = installation_manager->GetCurrentInstallationIndex();
  if (index == IM_EXT_ERROR) {
    LOG(ERROR) << "Updater failed to get current installation index.";
    return -1;
  }
  return index;
}

void UpdaterModule::SetMinFreeSpaceBytes(uint64_t bytes) {
  LOG(INFO) << "UpdaterModule::SetMinFreeSpaceBytes bytes=" << bytes;
  if (updater_configurator_) {
    updater_configurator_->SetMinFreeSpaceBytes(bytes);
  }
}

// TODO(b/244367569): refactor similar getter and setter methods in this class
// to share common code.
bool UpdaterModule::GetUseCompressedUpdates() const {
  LOG(INFO) << "UpdaterModule::GetUseCompressedUpdates";
  auto config = updater_configurator_;
  if (!config) {
    LOG(ERROR) << "UpdaterModule::GetUseCompressedUpdates: missing "
               << "configurator";
    return false;
  }

  bool use_compressed_updates = config->GetUseCompressedUpdates();
  LOG(INFO) << "UpdaterModule::GetUseCompressedUpdates use_compressed_updates="
            << use_compressed_updates;
  return use_compressed_updates;
}

void UpdaterModule::SetUseCompressedUpdates(bool use_compressed_updates) {
  auto config = updater_configurator_;
  if (!config) {
    LOG(ERROR) << "UpdaterModule::SetUseCompressedUpdates: missing "
               << "configurator";
    return;
  }

  LOG(INFO) << "UpdaterModule::SetUseCompressedUpdates use_compressed_updates="
            << use_compressed_updates;
  config->SetUseCompressedUpdates(use_compressed_updates);
}


}  // namespace updater
}  // namespace cobalt
