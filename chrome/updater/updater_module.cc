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

#include "chrome/updater/updater_module.h"

#include <utility>
#include <vector>

#include "base/bind.h"
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
#include "base/task/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/util.h"
#include "cobalt/base/task_runner_util.h"
#include "cobalt/browser/switches.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/cobalt_slot_management.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/installation_manager.h"

namespace {

using update_client::CobaltSlotManagement;
using update_client::ComponentState;
using update_client::UpdateCheckError;

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
    case ComponentState::kUpdated:
      return kCobaltExtensionUpdaterNotificationStateUpdated;
    case ComponentState::kUpToDate:
      return kCobaltExtensionUpdaterNotificationStateUpToDate;
    case ComponentState::kUpdateError:
      return kCobaltExtensionUpdaterNotificationStateUpdateFailed;
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
                   updater_status_string_map.at(UpdaterStatus::kUpdated)) == 0) {
      status = std::string(
          updater_status_string_map.at(UpdaterStatus::kUpdated));
    } else {
      status = std::string(
          updater_status_string_map.find(status_iterator->second)->second);
    }
    if (crx_update_item_.state == ComponentState::kUpdateError) {
      // QUICK_ROLL_FORWARD update, adjust the message to "Updated locally,
      // pending restart"
      if (crx_update_item_.error_code == static_cast<int>(UpdateCheckError::QUICK_ROLL_FORWARD)) {
        status = std::string(
            updater_status_string_map.at(UpdaterStatus::kRolledForward));
      } else {
        status +=
            ", error category is " + std::to_string(static_cast<int>(crx_update_item_.error_category)) +
            ",  error code is " + std::to_string(crx_update_item_.error_code);
      }
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
      base::Thread::Options(base::MessagePumpType::IO, 0));

  DETACH_FROM_THREAD(thread_checker_);
  // Initialize the underlying update client.
  is_updater_running_ = true;
  updater_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdaterModule::Initialize, base::Unretained(this)));
}

UpdaterModule::~UpdaterModule() {
  LOG(INFO) << "UpdaterModule::~UpdaterModule";
  if (is_updater_running_) {
    is_updater_running_ = false;
    base::task_runner_util::PostBlockingTask(
        updater_thread_->task_runner(), FROM_HERE,
        base::BindOnce(&UpdaterModule::Finalize, base::Unretained(this)));
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
    base::task_runner_util::PostBlockingTask(
        updater_thread_->task_runner(), FROM_HERE,
        base::BindOnce(&UpdaterModule::Finalize, base::Unretained(this)));
  }
}

void UpdaterModule::Resume() {
  if (!is_updater_running_) {
    is_updater_running_ = true;
    updater_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&UpdaterModule::Initialize, base::Unretained(this)));
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
      FROM_HERE, base::BindOnce(&UpdaterModule::Update, base::Unretained(this)),
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
      FROM_HERE, base::BindOnce(&UpdaterModule::MarkSuccessfulImpl,
                                base::Unretained(this)));
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

  // TODO(b/325626249): Remove the ALLOW_EVERGREEN_SIDELOADING check once we're
  // fully launched.
#if !defined(COBALT_BUILD_TYPE_GOLD) && ALLOW_EVERGREEN_SIDELOADING
  bool skip_verify_public_key_hash = GetAllowSelfSignedPackages();
  bool require_network_encryption = GetRequireNetworkEncryption();
#else
  bool skip_verify_public_key_hash = false;
  bool require_network_encryption = true;
#endif // defined(COBALT_BUILD_TYPE_GOLD)

  update_client_->Update(
      app_ids,
      base::BindOnce(
          [](base::Version manifest_version,
             bool skip_verify_public_key_hash,
             bool require_network_encryption,
             const std::vector<std::string>& ids)
              -> std::vector<base::Optional<update_client::CrxComponent>> {
            update_client::CrxComponent component;
            component.name = "cobalt";
            component.app_id = ids[0];
            component.version = manifest_version;
            component.pk_hash.assign(std::begin(kCobaltPublicKeyHash),
                                     std::end(kCobaltPublicKeyHash));
            component.requires_network_encryption = true;
            // TODO(b/325626249): Remove the ALLOW_EVERGREEN_SIDELOADING check
            // once we're fully launched.
#if !defined(COBALT_BUILD_TYPE_GOLD) && ALLOW_EVERGREEN_SIDELOADING
            if (skip_verify_public_key_hash) {
              component.pk_hash.clear();
            }
            component.requires_network_encryption = require_network_encryption;
#endif // !defined(COBALT_BUILD_TYPE_GOLD) && ALLOW_EVERGREEN_SIDELOADING
            component.crx_format_requirement = crx_file::VerifierFormat::CRX3;
            return {component};
          },
          manifest_version, skip_verify_public_key_hash, require_network_encryption),
      false,
      base::BindOnce(
          [](base::OnceClosure closure, update_client::Error error) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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
      FROM_HERE, base::BindOnce(&UpdaterModule::Update, base::Unretained(this)),
      base::TimeDelta::FromHours(kNextUpdateCheckHours));
}

// The following methods are called by other threads than the updater_thread_.

void UpdaterModule::CompareAndSwapForcedUpdate(int old_value, int new_value) {
  auto config = updater_configurator_;
  if (config) {
    config->CompareAndSwapForcedUpdate(old_value, new_value);
  }
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
  if (config) {
    config->SetChannel(updater_channel);
  }
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
      FROM_HERE,
      base::BindOnce(&UpdaterModule::Update, base::Unretained(this)));
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

bool UpdaterModule::GetAllowSelfSignedPackages() const {
  LOG(INFO) << "UpdaterModule::GetAllowSelfSignedPackages";
  auto config = updater_configurator_;
  if (!config) {
    LOG(ERROR) << "UpdaterModule::GetAllowSelfSignedPackages: missing configurator";
    return false;
  }

  bool allow_self_signed_builds = config->GetAllowSelfSignedPackages();
  LOG(INFO) << "UpdaterModule::GetAllowSelfSignedPackages allow_self_signed_builds="
            << allow_self_signed_builds;
  return allow_self_signed_builds;
}

void UpdaterModule::SetAllowSelfSignedPackages(bool allow_self_signed_builds) {
  LOG(INFO) << "UpdaterModule::SetAllowSelfSignedPackages";
  auto config = updater_configurator_;
  if (!config) {
    LOG(ERROR) << "UpdaterModule::SetAllowSelfSignedPackages: missing configurator";
    return;
  }

  config->SetAllowSelfSignedPackages(allow_self_signed_builds);
}

std::string UpdaterModule::GetUpdateServerUrl() const {
  LOG(INFO) << "UpdaterModule::GetUpdateServerUrl";
  auto config = updater_configurator_;
  if (!config) {
    LOG(ERROR) << "UpdaterModule::GetUpdateServerUrl: missing configurator";
    return "";
  }

  std::string update_server_url = config->GetUpdateServerUrl();
  LOG(INFO) << "UpdaterModule::GetUpdateServerUrl update_server_url="
            << update_server_url;
  return update_server_url;
}

void UpdaterModule::SetUpdateServerUrl(const std::string& update_server_url) {
  LOG(INFO) << "UpdaterModule::SetUpdateServerUrl";
  auto config = updater_configurator_;
  if(!config) {
    LOG(ERROR) << "UpdaterModule::SetUpdateServerUrl: missing configurator";
    return;
  }

  config->SetUpdateServerUrl(update_server_url);
}

bool UpdaterModule::GetRequireNetworkEncryption() const {
  LOG(INFO) << "UpdaterModule::GetRequireNetworkEncryption";
  auto config = updater_configurator_;
  if (!config) {
    LOG(ERROR) << "UpdaterModule::GetRequireNetworkEncryption: missing configurator";
    return true;
  }

  bool require_network_encryption = config->GetRequireNetworkEncryption();
  LOG(INFO) << "UpdaterModule::GetRequireNetworkEncryption require_network_encryption="
            << require_network_encryption;
  return require_network_encryption;
}

void UpdaterModule::SetRequireNetworkEncryption(bool require_network_encryption) {
  LOG(INFO) << "UpdaterModule::SetRequireNetworkEncryption";
  auto config = updater_configurator_;
  if (!config) {
    LOG(ERROR) << "UpdaterModule::SetRequireNetworkEncryption: missing configurator";
    return;
  }

  config->SetRequireNetworkEncryption(require_network_encryption);
}

}  // namespace updater
}  // namespace cobalt
